mat4 lidarProjectionMatrix()
{
  return uni.radar.viewProj * uni.radar.viewInverse;
}

vec3 lidarCameraForward()
{
  vec3 centerCamRay = (uni.radar.projInverse * vec4(0.0, 0.0, 1.0, 1.0)).xyz;
  return normalize(centerCamRay);
}

void lidarCameraBasis(out vec3 forwardCam, out vec3 rightCam, out vec3 upCam)
{
  forwardCam = lidarCameraForward();
  rightCam   = cross(vec3(0.0, 1.0, 0.0), forwardCam);
  if(length(rightCam) < 1e-6)
  {
    rightCam = vec3(1.0, 0.0, 0.0);
  }
  rightCam = normalize(rightCam);
  upCam    = normalize(cross(forwardCam, rightCam));
}

int nearestLidarVerticalChannel(float elevationDeg, out float snappedElevationDeg)
{
  float verticalMinDeg        = uni.radar.verticalParams.x;
  float verticalMaxDeg        = uni.radar.verticalParams.y;
  float verticalStepDeg       = uni.radar.verticalParams.z;
  float verticalStepAbsDeg    = max(abs(verticalStepDeg), 1e-4);
  float verticalMinBoundDeg   = min(verticalMinDeg, verticalMaxDeg);
  float verticalMaxBoundDeg   = max(verticalMinDeg, verticalMaxDeg);
  float verticalStepSign      = (verticalMaxDeg >= verticalMinDeg) ? 1.0 : -1.0;
  float verticalStepSignedDeg = verticalStepAbsDeg * verticalStepSign;
  float verticalSpanDeg       = abs(verticalMaxDeg - verticalMinDeg);
  int   channelCount          = max(int(floor(verticalSpanDeg / verticalStepAbsDeg)) + 1, 1);

  float channelFloat = (elevationDeg - verticalMinDeg) / verticalStepSignedDeg;
  int   channelIdx   = clamp(int(round(channelFloat)), 0, channelCount - 1);

  snappedElevationDeg = verticalMinDeg + float(channelIdx) * verticalStepSignedDeg;
  snappedElevationDeg = clamp(snappedElevationDeg, verticalMinBoundDeg, verticalMaxBoundDeg);
  return channelIdx;
}

vec3 lidarDirectionFromAngles(vec3 forwardCam, vec3 rightCam, vec3 upCam, float azimuthDeg, float elevationDeg)
{
  float azimuth       = radians(azimuthDeg);
  float elevation     = radians(elevationDeg);
  vec3  horizontalDir = normalize(cos(azimuth) * forwardCam + sin(azimuth) * rightCam);
  return normalize(cos(elevation) * horizontalDir + sin(elevation) * upCam);
}

bool projectCameraDirectionToPixel(vec3 dirCam, mat4 proj, out ivec2 outPixel)
{
  vec4 clip = proj * vec4(dirCam, 1.0);
  if(abs(clip.w) <= 1e-6)
  {
    return false;
  }

  vec2 ndc = clip.xy / clip.w;
  if(any(greaterThan(abs(ndc), vec2(1.0))))
  {
    return false;
  }

  vec2  pixelFloat = (ndc * 0.5 + 0.5) * vec2(gl_LaunchSizeEXT.xy) - vec2(0.5);
  ivec2 pixel      = ivec2(round(pixelFloat));
  if(any(lessThan(pixel, ivec2(0))) || any(greaterThanEqual(pixel, ivec2(gl_LaunchSizeEXT.xy))))
  {
    return false;
  }

  outPixel = pixel;
  return true;
}

void resetLidarPayload(uint seed)
{
  prd.radiance                  = vec3(0.0);
  prd.objId                     = -1;
  prd.instanceId                = -1;
  prd.throughput                = vec3(1.0);
  prd.seed                      = seed;
  prd.depth                     = 0;
  prd.done                      = 0;
  prd.firstHitWorldPosRoughness = vec4(0.0);
  prd.firstHitNormalSpecHitDist = vec4(0.0);
  prd.firstHitDiffuseValid      = vec4(0.0);
  prd.firstHitSpecularPad       = vec4(0.04, 0.04, 0.04, 0.0);
}

void traceLidarPointCloudAtPixel(ivec2 pixel, vec2 pixelCenter, vec3 fallbackColor, out vec3 lidarColor, out float lidarDistance)
{
  lidarColor    = fallbackColor;
  lidarDistance = 0.0;

  vec2 inUVNoJitter = pixelCenter / vec2(gl_LaunchSizeEXT.xy);
  vec2 dNoJitter    = inUVNoJitter * 2.0 - 1.0;
  vec3 pixelDirCam  = normalize((uni.radar.projInverse * vec4(dNoJitter, 1.0, 1.0)).xyz);
  vec3 forwardCam;
  vec3 rightCam;
  vec3 upCam;
  lidarCameraBasis(forwardCam, rightCam, upCam);

  float forwardComp  = dot(pixelDirCam, forwardCam);
  float rightComp    = dot(pixelDirCam, rightCam);
  float upComp       = dot(pixelDirCam, upCam);
  float horizLen     = max(length(vec2(forwardComp, rightComp)), 1e-6);
  float azimuthDeg   = degrees(atan(rightComp, forwardComp));
  float elevationDeg = degrees(atan(upComp, horizLen));

  float lidarAzimuthMinDeg   = min(uni.radar.azimuthParams.x, uni.radar.azimuthParams.y);
  float lidarAzimuthMaxDeg   = max(uni.radar.azimuthParams.x, uni.radar.azimuthParams.y);
  float lidarAzimuthStepDeg  = max(abs(uni.radar.azimuthParams.z), 1e-4);
  float lidarAzimuthSpanDeg  = lidarAzimuthMaxDeg - lidarAzimuthMinDeg;
  float lidarPointRadius     = max(uni.radar.azimuthParams.w, 0.0);
  int   lidarHorizontalCount = max(int(floor(lidarAzimuthSpanDeg / lidarAzimuthStepDeg)) + 1, 1);

  if(azimuthDeg < (lidarAzimuthMinDeg - 0.5) || azimuthDeg > (lidarAzimuthMaxDeg + 0.5))
  {
    return;
  }

  int azimuthIdx = int(round((azimuthDeg - lidarAzimuthMinDeg) / lidarAzimuthStepDeg));
  azimuthIdx     = clamp(azimuthIdx, 0, lidarHorizontalCount - 1);

  float snappedElDeg = 0.0;
  int   channelIdx   = nearestLidarVerticalChannel(elevationDeg, snappedElDeg);
  float snappedAzDeg = lidarAzimuthMinDeg + float(azimuthIdx) * lidarAzimuthStepDeg;

  vec3  beamDirCam = lidarDirectionFromAngles(forwardCam, rightCam, upCam, snappedAzDeg, snappedElDeg);
  mat4  proj       = lidarProjectionMatrix();
  ivec2 beamPixel  = ivec2(-1);
  if(!projectCameraDirectionToPixel(beamDirCam, proj, beamPixel))
  {
    return;
  }

  vec2  pixelDelta   = vec2(pixel - beamPixel);
  float pixelDist2   = dot(pixelDelta, pixelDelta);
  float pointRadius2 = lidarPointRadius * lidarPointRadius;
  if(pixelDist2 > pointRadius2)
  {
    return;
  }

  vec3 lidarOriginWorld = (uni.radar.viewInverse * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
  vec3 lidarDirWorld    = normalize((uni.radar.viewInverse * vec4(beamDirCam, 0.0)).xyz);

  uint lidarSeed = initRng(uvec2(uint(azimuthIdx), uint(channelIdx)), pcRay.frameIndex, 0u);
  resetLidarPayload(lidarSeed);
  traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, lidarOriginWorld, 0.001, lidarDirWorld, 10000.0, 0);

  if(prd.firstHitDiffuseValid.w > 0.5)
  {
    vec3 hitCam   = (uni.radar.view * vec4(prd.firstHitWorldPosRoughness.xyz, 1.0)).xyz;
    lidarDistance = length(hitCam);
    lidarColor    = vec3(1.0, 0.0, 0.0);
  }
}
