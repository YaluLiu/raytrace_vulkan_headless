#version 460
#extension GL_EXT_multiview : require
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

#include "host_device.h"

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec4 inTangent;

layout(set = 0, binding = eTileFrameUniforms) uniform _TileFrameUniforms
{
  TileFrameUniforms tileFrame;
};
layout(push_constant) uniform _MeshDrawPushConstants
{
  MeshDrawPushConstants pc;
};

layout(location = 0) out vec3 outWorldPos;
layout(location = 1) out vec3 outWorldNormal;
layout(location = 2) out vec3 outColor;
layout(location = 3) out vec2 outTexCoord;
layout(location = 4) out vec4 outWorldTangent;
layout(location = 5) flat out uint outObjIndex;

void main()
{
  uint viewIndex = min(uint(gl_ViewIndex), max(tileFrame.viewCount, 1u) - 1u);
  vec4 worldPos = pc.model * vec4(inPosition, 1.0);
  mat3 normalMatrix = transpose(inverse(mat3(pc.model)));

  outWorldPos = worldPos.xyz;
  outWorldNormal = normalize(normalMatrix * inNormal);
  outColor = inColor;
  outTexCoord = inTexCoord;
  outWorldTangent = vec4(normalize(mat3(pc.model) * inTangent.xyz), inTangent.w);
  outObjIndex = pc.objIndex;

  gl_Position = tileFrame.cameras[viewIndex].viewProj * worldPos;
}
