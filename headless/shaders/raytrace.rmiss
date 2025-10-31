#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require

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

  // 遍历所有光源，查找 Dome Light
  for(int i = 0; i < numLights; i++)
  {
    Light light = lightBuf.lights[i];

    if(light.type == 2)  // Dome light
    {
      vec3 lightEmission = vec3(0.0);

      // 如果有环境贴图
      if(light.textureID >= 1000)
      {
        lightEmission = vec3(1.0, 0.0, 0.0);
      }
      else if(light.textureID >= 0)
      {
        uint txtId = light.textureID;

        // 应用旋转到射线方向
        vec3 rotatedDir = rotateByQuaternion(rayDir, light.rotateQuat);

        // 计算球面坐标 UV
        float theta = acos(clamp(rotatedDir.y, -1.0, 1.0));
        float phi   = atan(rotatedDir.z, rotatedDir.x);

        vec2 texCoord = vec2(phi / (2.0 * 3.14159265359) + 0.5, theta / 3.14159265359);

        // 采样环境贴图
        vec3 textureColor = texture(textureSamplers[nonuniformEXT(txtId)], texCoord).xyz;

        // 应用基础发光和贴图颜色
        lightEmission = light.baseEmission * textureColor;
      }
      else
      {
        // 没有贴图时使用基础发光
        lightEmission = light.baseEmission;
      }

      // 累加环境光（可能有多个 Dome Light）
      envColor += lightEmission;
    }
  }

  // 如果没有找到 Dome Light，使用默认背景色
  if(envColor == vec3(0.0))
  {
    // 可以设置一个默认的天空颜色或纯色
    envColor = vec3(0.1, 0.1, 0.15);  // 深蓝色背景
  }

  prd.hitValue   = envColor;
  prd.objId      = -1;
  prd.instanceId = -1;
}
