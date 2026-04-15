mat4 lidarProjectionMatrix()
{
  return uni.viewProj * uni.viewInverse;
}

vec3 lidarCameraForward()
{
  vec3 centerCamRay = (uni.projInverse * vec4(0.0, 0.0, 1.0, 1.0)).xyz;
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

int nearestLidarVerticalChannel(float elevationDeg)
{
  int channelCount = clamp(pcRay.lidar.verticalChannelCount, 1, LIDAR_VERTICAL_CHANNEL_CAPACITY);
  int   bestIdx  = 0;
  float bestDiff = abs(elevationDeg - pcRay.lidar.verticalAnglesDeg[0]);
  for(int i = 1; i < channelCount; ++i)
  {
    float diff = abs(elevationDeg - pcRay.lidar.verticalAnglesDeg[i]);
    if(diff < bestDiff)
    {
      bestDiff = diff;
      bestIdx  = i;
    }
  }
  return bestIdx;
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
  vec3 pixelDirCam  = normalize((uni.projInverse * vec4(dNoJitter, 1.0, 1.0)).xyz);
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

  float lidarAzimuthMinDeg   = min(pcRay.lidar.azimuthMinDeg, pcRay.lidar.azimuthMaxDeg);
  float lidarAzimuthMaxDeg   = max(pcRay.lidar.azimuthMinDeg, pcRay.lidar.azimuthMaxDeg);
  float lidarAzimuthStepDeg  = max(abs(pcRay.lidar.azimuthStepDeg), 1e-4);
  float lidarPointRadius     = max(pcRay.lidar.pointRadiusPixels, 0.0);
  int   lidarHorizontalCount = max(pcRay.lidar.horizontalSampleCount, 1);

  if(azimuthDeg < (lidarAzimuthMinDeg - 0.5) || azimuthDeg > (lidarAzimuthMaxDeg + 0.5))
  {
    return;
  }

  int azimuthIdx = int(round((azimuthDeg - lidarAzimuthMinDeg) / lidarAzimuthStepDeg));
  azimuthIdx     = clamp(azimuthIdx, 0, lidarHorizontalCount - 1);

  int   channelIdx   = nearestLidarVerticalChannel(elevationDeg);
  float snappedAzDeg = lidarAzimuthMinDeg + float(azimuthIdx) * lidarAzimuthStepDeg;
  float snappedElDeg = pcRay.lidar.verticalAnglesDeg[channelIdx];

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

  vec3 lidarOriginWorld = (uni.viewInverse * vec4(0.0, 0.0, 0.0, 1.0)).xyz;
  vec3 lidarDirWorld    = normalize((uni.viewInverse * vec4(beamDirCam, 0.0)).xyz);

  uint lidarSeed = initRng(uvec2(uint(azimuthIdx), uint(channelIdx)), pcRay.frameIndex, 0u);
  resetLidarPayload(lidarSeed);
  traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, lidarOriginWorld, 0.001, lidarDirWorld, 10000.0, 0);

  if(prd.firstHitDiffuseValid.w > 0.5)
  {
    vec3 hitCam   = (uni.view * vec4(prd.firstHitWorldPosRoughness.xyz, 1.0)).xyz;
    lidarDistance = length(hitCam);
    lidarColor    = vec3(1.0, 0.0, 0.0);
  }
}
