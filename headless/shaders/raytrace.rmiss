#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_scalar_block_layout : enable  // 添加这一行

#include "raycommon.glsl"
#include "wavefront.glsl"

layout(location = 0) rayPayloadInEXT hitPayload prd;

layout(set = 1, binding = eTextures) uniform sampler2D textureSamplers[];
layout(set = 1, binding = eLights, scalar) buffer LightBuf
{
  Light lights[];
}
lightBuf;
layout(push_constant) uniform _PushConstantRay
{
  PushConstantRay pcRay;
};

vec3 rotateByQuaternion(vec3 v, vec4 q)
{
  vec3  u = q.xyz;
  float s = q.w;
  return 2.0 * dot(u, v) * u + (s * s - dot(u, u)) * v + 2.0 * s * cross(u, v);
}

void main()
{
  vec3 rayDir   = normalize(gl_WorldRayDirectionEXT);
  vec3 envColor = vec3(0.0);

  int numLights = pcRay.numLights;

  for(int i = 0; i < numLights; i++)
  {
    Light light = lightBuf.lights[i];

    if(light.type == 2)  // Dome light
    {
      vec3 lightEmission = vec3(0.0);
      if(light.textureID >= 0)
      {
        uint txtId = light.textureID;

        // 应用旋转到射线方向
        vec3 rotatedDir = rotateByQuaternion(rayDir, light.rotateQuat);

        // 计算球面坐标 UV
        float theta = acos(clamp(rotatedDir.y, -1.0, 1.0));
        float phi   = atan(rotatedDir.z, rotatedDir.x);

        vec2 texCoord     = vec2(phi / (2.0 * 3.14159265359) + 0.5, theta / 3.14159265359);
        vec3 textureColor = texture(textureSamplers[nonuniformEXT(txtId)], texCoord).xyz;
        lightEmission     = light.baseEmission * textureColor;
      }
      else
      {
        lightEmission = light.baseEmission;
      }

      envColor += lightEmission;
    }
  }

  if(envColor == vec3(0.0))
  {
    envColor = vec3(0.18, 0.18, 0.18);
  }

  prd.hitValue   = envColor;
  prd.objId      = -1;
  prd.instanceId = -1;
}
