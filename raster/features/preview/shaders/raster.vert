#version 460
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "host_device.h"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec4 inTangent;

layout(set = 0, binding = eFrameUniforms) uniform _FrameUniforms
{
  FrameUniforms frameUni;
};
layout(push_constant) uniform _PushConstantRaster
{
  PushConstantRaster pcRaster;
};

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outWorldNormal;
layout(location = 2) out vec3 outColor;
layout(location = 3) out vec2 outTexCoord;
layout(location = 4) out vec4 outWorldTangent;
layout(location = 5) flat out uint outObjIndex;
layout(location = 6) flat out int outInstanceId;

void main()
{
  vec4 worldPos = pcRaster.model * vec4(inPosition, 1.0);
  mat3 normalMatrix = transpose(inverse(mat3(pcRaster.model)));

  outWorldPos = worldPos.xyz;
  outWorldNormal = normalize(normalMatrix * inNormal);
  outColor = inColor;
  outTexCoord = inTexCoord;
  outWorldTangent = vec4(normalize(mat3(pcRaster.model) * inTangent.xyz), inTangent.w);
  outObjIndex = pcRaster.objIndex;
  outInstanceId = pcRaster.instanceId;

  gl_Position = frameUni.camera.viewProj * worldPos;
}
