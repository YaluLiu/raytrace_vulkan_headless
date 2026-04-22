const float kLidarMinStepDeg = 1e-4;
const float kLidarRayTMin    = 0.001;

int lidarAxisSampleCount(float minDeg, float maxDeg, float stepDeg)
{
  float absStep = max(abs(stepDeg), kLidarMinStepDeg);
  float span    = abs(maxDeg - minDeg);
  int   steps   = int(floor(span / absStep + 0.5));
  return max(steps + 1, 1);
}

float lidarAngleAtIndex(float minDeg, float maxDeg, float stepDeg, int idx)
{
  int count = lidarAxisSampleCount(minDeg, maxDeg, stepDeg);
  if(count <= 1)
  {
    return minDeg;
  }

  float stepMag = max(abs(stepDeg), kLidarMinStepDeg);
  float stepDir = (maxDeg >= minDeg) ? 1.0 : -1.0;
  float value   = minDeg + float(clamp(idx, 0, count - 1)) * stepDir * stepMag;
  return clamp(value, min(minDeg, maxDeg), max(minDeg, maxDeg));
}

void lidarRayIndexToAngles(uint rayIdx, out float azDeg, out float elDeg)
{
  int azCount = lidarAxisSampleCount(pcRay.lidar.azimuthMinDeg, pcRay.lidar.azimuthMaxDeg, pcRay.lidar.azimuthStepDeg);
  int elCount = lidarAxisSampleCount(pcRay.lidar.verticalMinDeg, pcRay.lidar.verticalMaxDeg, pcRay.lidar.verticalStepDeg);
  int total   = max(azCount * elCount, 1);

  int clampedRayIdx = int(min(rayIdx, uint(total - 1)));
  int azIdx         = clampedRayIdx % azCount;
  int elIdx         = clampedRayIdx / azCount;

  azDeg = lidarAngleAtIndex(pcRay.lidar.azimuthMinDeg, pcRay.lidar.azimuthMaxDeg, pcRay.lidar.azimuthStepDeg, azIdx);
  elDeg = lidarAngleAtIndex(pcRay.lidar.verticalMinDeg, pcRay.lidar.verticalMaxDeg, pcRay.lidar.verticalStepDeg, elIdx);
}

void buildLidarWorldRay(float azDeg, float elDeg, out vec3 origin, out vec3 direction)
{
  float azRad = radians(azDeg);
  float elRad = radians(elDeg);
  float cosEl = cos(elRad);

  // Camera space convention: +X right, +Y up, -Z forward.
  vec3 localDirection = normalize(vec3(sin(azRad) * cosEl, sin(elRad), -cos(azRad) * cosEl));

  origin    = frameUni.lidar.params.positionAndPad.xyz;
  direction = normalize((frameUni.lidar.camera.viewInverse * vec4(localDirection, 0.0)).xyz);
}

bool traceLidarHit(uint rayIdx, out vec3 worldHit)
{
  float azDeg;
  float elDeg;
  lidarRayIndexToAngles(rayIdx, azDeg, elDeg);

  vec3 origin;
  vec3 direction;
  buildLidarWorldRay(azDeg, elDeg, origin, direction);

  prd.radiance                  = vec3(0.0);
  prd.objId                     = -1;
  prd.instanceId                = -1;
  prd.throughput                = vec3(1.0);
  prd.seed                      = rayIdx * 1664525u + 1013904223u;
  prd.depth                     = max(pcRay.maxDepth, 1) - 1;
  prd.done                      = 0;
  prd.firstHitWorldPosRoughness = vec4(0.0);
  prd.firstHitNormalSpecHitDist = vec4(0.0);
  prd.firstHitDiffuseValid      = vec4(0.0);
  prd.firstHitSpecularPad       = vec4(0.0);

  float tMax = max(pcRay.lidar.maxDistance, kLidarRayTMin);
  traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, origin, kLidarRayTMin, direction, tMax, 0);

  if(prd.firstHitDiffuseValid.w <= 0.5)
  {
    worldHit = vec3(0.0);
    return false;
  }

  worldHit = prd.firstHitWorldPosRoughness.xyz;
  return true;
}

bool projectWorldToMainPixel(vec3 worldPos, out ivec2 screenPixel)
{
  vec4 clipPos = frameUni.camera.viewProj * vec4(worldPos, 1.0);
  if(clipPos.w <= 1e-6)
  {
    screenPixel = ivec2(0);
    return false;
  }

  vec3 ndc = clipPos.xyz / clipPos.w;
  if(ndc.z < 0.0 || ndc.z > 1.0 || ndc.x < -1.0 || ndc.x > 1.0 || ndc.y < -1.0 || ndc.y > 1.0)
  {
    screenPixel = ivec2(0);
    return false;
  }

  ivec2 imageExtent = imageSize(lidarPointCloudImage);
  vec2  uv          = ndc.xy * 0.5 + vec2(0.5);
  ivec2 pixel       = ivec2(floor(uv * vec2(imageExtent)));
  if(any(lessThan(pixel, ivec2(0))) || any(greaterThanEqual(pixel, imageExtent)))
  {
    screenPixel = ivec2(0);
    return false;
  }

  screenPixel = pixel;
  return true;
}

void splatPoint(ivec2 centerPixel, float radiusPx, vec3 color)
{
  ivec2 imageExtent = imageSize(lidarPointCloudImage);
  float radius      = max(radiusPx, 0.5);
  int   radiusInt   = int(ceil(radius));
  float radiusSq    = radius * radius;

  for(int y = -radiusInt; y <= radiusInt; ++y)
  {
    for(int x = -radiusInt; x <= radiusInt; ++x)
    {
      vec2 footprintOffset = vec2(float(x), float(y));
      if(dot(footprintOffset, footprintOffset) > radiusSq)
      {
        continue;
      }

      ivec2 dstPixel = centerPixel + ivec2(x, y);
      if(any(lessThan(dstPixel, ivec2(0))) || any(greaterThanEqual(dstPixel, imageExtent)))
      {
        continue;
      }

      imageStore(lidarPointCloudImage, dstPixel, vec4(color, 1.0));
    }
  }
}

void traceLidarPointCloudAtPixel(ivec2 pixel, vec2 pixelCenter, vec3 fallbackColor, out vec3 lidarColor)
{
  lidarColor = fallbackColor;

  int azCount   = lidarAxisSampleCount(pcRay.lidar.azimuthMinDeg, pcRay.lidar.azimuthMaxDeg, pcRay.lidar.azimuthStepDeg);
  int elCount   = lidarAxisSampleCount(pcRay.lidar.verticalMinDeg, pcRay.lidar.verticalMaxDeg, pcRay.lidar.verticalStepDeg);
  int totalRays = azCount * elCount;
  if(totalRays <= 0)
  {
    return;
  }

  ivec2 imageExtent = imageSize(lidarPointCloudImage);
  uint  rayIdx      = uint(pixel.y * imageExtent.x + pixel.x);
  if(rayIdx >= uint(totalRays))
  {
    return;
  }

  vec3 worldHit;
  if(!traceLidarHit(rayIdx, worldHit))
  {
    return;
  }

  ivec2 projectedPixel;
  if(!projectWorldToMainPixel(worldHit, projectedPixel))
  {
    return;
  }

  if(all(equal(projectedPixel, pixel)))
  {
    lidarColor = vec3(1.0, 0.0, 0.0);
  }
}
