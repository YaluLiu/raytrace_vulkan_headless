#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "host_device.h"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec4 inTangent;

layout(push_constant) uniform _PushConstantLidarDepth
{
  PushConstantLidarDepth pc;
};

layout(location = 0) out float outRangeMeters;

float signedStep(float startDeg, float endDeg, float absStep)
{
  return endDeg >= startDeg ? abs(absStep) : -abs(absStep);
}

void main()
{
  vec4 worldPos = pc.model * vec4(inPosition, 1.0);
  vec3 origin = pc.originMaxRange.xyz;
  float maxRange = max(pc.originMaxRange.w, 1.0e-4);
  vec3 rel = worldPos.xyz - origin;

  vec3 forward = normalize(pc.forwardAzimuthStart.xyz);
  vec3 right = normalize(pc.rightAzimuthStep.xyz);
  vec3 up = normalize(pc.upVerticalStart.xyz);
  float forwardMeters = dot(rel, forward);
  float rightMeters = dot(rel, right);
  float upMeters = dot(rel, up);
  float planarMeters = length(vec2(rightMeters, forwardMeters));
  float rangeMeters = length(rel);

  float azimuthDeg = degrees(atan(rightMeters, forwardMeters));
  float verticalDeg = degrees(atan(upMeters, max(planarMeters, 1.0e-6)));
  float azimuthStep = signedStep(pc.forwardAzimuthStart.w, pc.azimuthEndVerticalEndStep.x, pc.rightAzimuthStep.w);
  float verticalStep = signedStep(pc.upVerticalStart.w, pc.azimuthEndVerticalEndStep.y, pc.azimuthEndVerticalEndStep.z);

  float azimuthCoord = (azimuthDeg - pc.forwardAzimuthStart.w) / max(abs(azimuthStep), 1.0e-4);
  if(azimuthStep < 0.0)
  {
    azimuthCoord = (pc.forwardAzimuthStart.w - azimuthDeg) / max(abs(azimuthStep), 1.0e-4);
  }
  float verticalCoord = (verticalDeg - pc.upVerticalStart.w) / max(abs(verticalStep), 1.0e-4);
  if(verticalStep < 0.0)
  {
    verticalCoord = (pc.upVerticalStart.w - verticalDeg) / max(abs(verticalStep), 1.0e-4);
  }

  float maxAzimuthCoord = max(float(pc.azimuthSampleCount) - 1.0, 1.0);
  float maxVerticalCoord = max(float(pc.verticalSampleCount) - 1.0, 1.0);
  bool outside = forwardMeters <= 0.0 || rangeMeters <= 0.0 || rangeMeters > maxRange ||
                 azimuthCoord < -0.5 || verticalCoord < -0.5 ||
                 azimuthCoord > maxAzimuthCoord + 0.5 || verticalCoord > maxVerticalCoord + 0.5;

  if(outside)
  {
    gl_Position = vec4(2.0, 2.0, 1.0, 1.0);
    outRangeMeters = 0.0;
    return;
  }

  vec2 uv = vec2(azimuthCoord / maxAzimuthCoord, verticalCoord / maxVerticalCoord);
  outRangeMeters = rangeMeters;
  gl_Position = vec4(uv * 2.0 - 1.0, clamp(rangeMeters / maxRange, 0.0, 1.0), 1.0);
}
