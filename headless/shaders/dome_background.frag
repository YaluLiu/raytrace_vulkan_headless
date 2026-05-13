#version 460
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "host_device.h"

layout(set = 0, binding = eFrameUniforms) uniform _FrameUniforms
{
  FrameUniforms frameUni;
};
layout(set = 0, binding = eLights, scalar) readonly buffer Lights_
{
  Light i[];
}
lights;
layout(set = 0, binding = eTextures) uniform sampler2D textureSamplers[];

#include "dome_light.glsl"

layout(location = 0) in vec2 inNdc;
layout(location = 0) out vec4 outColor;
layout(location = 1) out int outObjId;
layout(location = 2) out int outInstanceId;
layout(location = 3) out float outDepth;

vec3 reconstructWorldDirection(vec2 ndc)
{
  vec4 viewTarget = frameUni.camera.projInverse * vec4(ndc, 1.0, 1.0);
  float invW = abs(viewTarget.w) > 1.0e-6 ? 1.0 / viewTarget.w : 1.0;
  vec3 viewDir = normalize(viewTarget.xyz * invW);
  return normalize((frameUni.camera.viewInverse * vec4(viewDir, 0.0)).xyz);
}

void main()
{
  Light domeLight;
  vec3 color = vec3(0.0);
  if(getFirstDomeLight(domeLight))
  {
    color = sampleDomeLight(domeLight, reconstructWorldDirection(inNdc));
  }
  outColor = vec4(color, 1.0);
  outObjId = -1;
  outInstanceId = -1;
  outDepth = 1.0;
}
