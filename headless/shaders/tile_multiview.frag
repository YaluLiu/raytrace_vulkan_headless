#version 460
#extension GL_EXT_multiview : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "host_device.h"

layout(buffer_reference, scalar) buffer Materials
{
  WaveFrontMaterial m[];
};
layout(buffer_reference, scalar) buffer MatIndices
{
  int i[];
};

layout(set = 0, binding = eObjDescs, scalar) buffer ObjDesc_
{
  ObjDesc i[];
}
objDesc;
layout(set = 0, binding = eTileFrameUniforms) uniform _TileFrameUniforms
{
  TileFrameUniforms tileFrame;
};
layout(set = 0, binding = eLights, scalar) readonly buffer Lights_
{
  Light i[];
}
lights;
layout(set = 0, binding = eTextures) uniform sampler2D textureSamplers[];

#define DOME_LIGHT_COUNT tileFrame.lightCount
#include "dome_light.glsl"

layout(location = 0) in vec3 inWorldPos;
layout(location = 1) in vec3 inWorldNormal;
layout(location = 2) in vec3 inColor;
layout(location = 3) in vec2 inTexCoord;
layout(location = 4) in vec4 inWorldTangent;
layout(location = 5) flat in uint inObjIndex;
layout(location = 6) flat in int inInstanceId;

layout(location = 0) out vec4 outColor;
layout(location = 1) out int outObjId;
layout(location = 2) out int outInstanceId;
layout(location = 3) out float outDepth;

vec4 sampleTextureRgba(int textureId, vec2 texCoord, vec4 fallbackValue)
{
  if(textureId < 0)
  {
    return fallbackValue;
  }
  return texture(textureSamplers[nonuniformEXT(uint(textureId))], texCoord);
}

vec3 sampleBaseColor(WaveFrontMaterial mat, vec2 texCoord, vec3 fallbackColor)
{
  vec3 baseColor = clamp(mat.baseColorFactor, vec3(0.0), vec3(1.0));
  if(max(baseColor.r, max(baseColor.g, baseColor.b)) <= 1.0e-5)
  {
    baseColor = clamp(fallbackColor, vec3(0.0), vec3(1.0));
  }

  if(mat.baseColorTextureId >= 0)
  {
    baseColor *= sampleTextureRgba(mat.baseColorTextureId, texCoord, vec4(1.0)).rgb;
  }
  else if(mat.diffuseTextureId >= 0)
  {
    baseColor *= sampleTextureRgba(mat.diffuseTextureId, texCoord, vec4(1.0)).rgb;
  }

  return clamp(baseColor + mat.emissionFactor, vec3(0.0), vec3(32.0));
}

vec3 evaluateSphereLight(Light light, vec3 normal, vec3 worldPos)
{
  vec3 toLight = light.position - worldPos;
  float distanceSq = max(dot(toLight, toLight), 1.0e-6);
  vec3 lightDir = toLight * inversesqrt(distanceSq);
  float lambert = max(dot(normal, lightDir), 0.0);
  if(lambert <= 0.0)
  {
    return vec3(0.0);
  }

  float radius = max(light.radius, 0.0);
  float attenuation = 1.0 / max(1.0, max(distanceSq, radius * radius));
  return max(light.diffuse, 0.0) * light.baseEmission * lambert * attenuation;
}

vec3 evaluateFallbackLighting(vec3 normal)
{
  vec3 lightDir = normalize(vec3(0.35, 0.85, 0.4));
  float lambert = max(dot(normal, lightDir), 0.0);
  return vec3(0.18 + 0.82 * lambert);
}

void main()
{
  ObjDesc objResource = objDesc.i[inObjIndex];
  Materials materials = Materials(objResource.materialAddress);
  MatIndices matIndices = MatIndices(objResource.materialIndexAddress);

  int matIdx = matIndices.i[gl_PrimitiveID];
  WaveFrontMaterial mat = materials.m[matIdx];

  vec3 normal = normalize(inWorldNormal);
  if(length(normal) < 1.0e-5)
  {
    normal = vec3(0.0, 1.0, 0.0);
  }

  vec3 baseColor = sampleBaseColor(mat, inTexCoord, inColor);
  vec3 lighting = vec3(0.0);
  int supportedLightCount = 0;
  uint lightCount = min(tileFrame.lightCount, MAX_SCENE_LIGHTS);
  for(uint lightIndex = 0; lightIndex < lightCount; ++lightIndex)
  {
    Light light = lights.i[lightIndex];
    if(light.type == LIGHT_TYPE_SPHERE)
    {
      lighting += evaluateSphereLight(light, normal, inWorldPos);
      supportedLightCount++;
    }
    else if(light.type == LIGHT_TYPE_DOME)
    {
      lighting += sampleDomeLightDiffuse(light, normal) * max(light.diffuse, 0.0);
      supportedLightCount++;
    }
  }

  if(supportedLightCount == 0)
  {
    lighting = evaluateFallbackLighting(normal);
  }

  vec3 litColor = baseColor * lighting;

  outColor = vec4(litColor, 1.0);
  outObjId = int(inObjIndex);
  outInstanceId = inInstanceId;
  outDepth = gl_FragCoord.z;
}
