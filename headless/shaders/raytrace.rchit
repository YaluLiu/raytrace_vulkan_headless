#version 460
#extension GL_EXT_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_EXT_scalar_block_layout : enable
#extension GL_GOOGLE_include_directive : enable

#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require
#extension GL_EXT_buffer_reference2 : require

#include "raycommon.glsl"
#include "wavefront.glsl"

hitAttributeEXT vec2 attribs;

// clang-format off
layout(location = 0) rayPayloadInEXT hitPayload prd;
layout(location = 1) rayPayloadEXT bool isShadowed;

layout(buffer_reference, scalar) buffer Vertices {Vertex v[]; }; // Positions of an object
layout(buffer_reference, scalar) buffer Indices {ivec3 i[]; }; // Triangle indices
layout(buffer_reference, scalar) buffer Materials {WaveFrontMaterial m[]; }; // Array of all materials on an object
layout(buffer_reference, scalar) buffer MatIndices {int i[]; }; // Material ID for each triangle
layout(set = 0, binding = eTlas) uniform accelerationStructureEXT topLevelAS;
layout(set = 1, binding = eObjDescs, scalar) buffer ObjDesc_ { ObjDesc i[]; } objDesc;
layout(set = 1, binding = eTextures) uniform sampler2D textureSamplers[];
layout(set = 1, binding = eLights, scalar) buffer LightBuf { Light lights[]; } lightBuf;
layout(set = 1, binding = eInstanceIds, scalar) buffer InstanceIdBuf { int instanceIds[]; } instanceIdBuf; // 新增
layout(push_constant) uniform _PushConstantRay { PushConstantRay pcRay; };

vec3 rotateByQuaternion(vec3 v, vec4 q) 
{
  vec3 u = q.xyz;
  float s = q.w;
  return 2.0 * dot(u, v) * u + (s * s - dot(u, u)) * v + 2.0 * s * cross(u, v);
}

void main()
{
  // Object data
  ObjDesc    objResource = objDesc.i[gl_InstanceCustomIndexEXT];
  MatIndices matIndices  = MatIndices(objResource.materialIndexAddress);
  Materials  materials   = Materials(objResource.materialAddress);
  Indices    indices     = Indices(objResource.indexAddress);
  Vertices   vertices    = Vertices(objResource.vertexAddress);

  // Indices of the triangle
  ivec3 ind = indices.i[gl_PrimitiveID];

  // Vertex of the triangle
  Vertex v0 = vertices.v[ind.x];
  Vertex v1 = vertices.v[ind.y];
  Vertex v2 = vertices.v[ind.z];

  const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

  // Computing the coordinates of the hit position
  const vec3 pos      = v0.pos * barycentrics.x + v1.pos * barycentrics.y + v2.pos * barycentrics.z;
  const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));  // Transforming the position to world space

  // Computing the normal at hit position
  const vec3 nrm      = v0.nrm * barycentrics.x + v1.nrm * barycentrics.y + v2.nrm * barycentrics.z;
  const vec3 worldNrm = normalize(vec3(nrm * gl_WorldToObjectEXT));  // Transforming the normal to world space

  // Material of the object
  int               matIdx = matIndices.i[gl_PrimitiveID];
  WaveFrontMaterial mat    = materials.m[matIdx];

  vec3 textureColor = vec3(1.0);
  if(mat.textureId >= 0)
  {
    uint txtId    = mat.textureId;// + objDesc.i[gl_InstanceCustomIndexEXT].txtOffset;
    vec2 texCoord = v0.texCoord * barycentrics.x + v1.texCoord * barycentrics.y + v2.texCoord * barycentrics.z;
    textureColor  = texture(textureSamplers[nonuniformEXT(txtId)], texCoord).xyz;
  }

  vec3 emission = computeEmission(mat, textureColor);
  vec3 totalLight = vec3(0);
  int  numLights  = pcRay.numLights;

  for(int i = 0; i < numLights; i++)
  {
    Light light = lightBuf.lights[i];
    vec3  L;
    float lightDistance       = 100000.0;
    float distanceAttenuation = 1.0; //距离衰减系数

    if(light.type == 0)  // Sphere light
    {
      vec3 lDir     = light.position.xyz - worldPos;
      lightDistance = length(lDir);
      float effectiveDistance = max(lightDistance - light.radius, 0.001);
      distanceAttenuation     = 1.0 / (effectiveDistance * effectiveDistance);

      L = normalize(lDir);
    }
    else if(light.type == 1)  // Distant light
    {
      L = normalize(light.direction.xyz);
      distanceAttenuation = light.angleScale;
    }
    else if(light.type == 2)  // Dome light
    {
      // 使用法线采样半球环境光
      vec3 upVector = vec3(0, 1, 0);

      // 使用四元数旋转 up vector，得到 dome 的实际"上"方向
      vec3 domeUp = rotateByQuaternion(upVector, light.rotateQuat);

      // 计算法线与dome上方向的对齐程度
      float alignment = dot(worldNrm, domeUp);

      // 基于对齐程度计算光照方向和强度
      // 当表面朝向dome顶部时获得最强光照
      L = normalize(mix(worldNrm, domeUp, 0.5));  // 混合法线和dome方向

      // Dome light的光照强度基于表面法线与dome方向的夹角
      // 使用cosine分布来模拟半球光照
      distanceAttenuation = max(alignment * 0.5 + 0.5, 0.1);  // 映射到[0.1, 1]范围

      // 对于背向dome的表面，仍然给予一些环境光
      if(alignment < 0)
      {
        distanceAttenuation *= 0.3;  // 背面接收较少的光
      }
    }

    // 使用预计算的baseEmission
    vec3 lightEmission = light.baseEmission * distanceAttenuation;

    // 计算漫反射
    vec3 diffuse = computeDiffuse(mat, L, worldNrm);
    diffuse *= textureColor * light.diffuseScale;

    vec3  specular          = vec3(0);
    float shadowAttenuation = 1.0;

    // 阴影检测 - Dome light 不产生阴影
    if(light.type != 2 && dot(worldNrm, L) > 0)  // Dome light跳过阴影
    {
      float tMin   = 0.001;
      float tMax   = lightDistance;
      vec3  origin = gl_WorldRayOriginEXT + gl_WorldRayDirectionEXT * gl_HitTEXT;
      vec3  rayDir = L;
      uint  flags  = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
      isShadowed   = true;
      traceRayEXT(topLevelAS, flags, 0xFF, 0, 0, 1, origin, tMin, rayDir, tMax, 1);

      if(isShadowed)
      {
        shadowAttenuation = 0.3;
      }
      else
      {
        specular = computeSpecular(mat, gl_WorldRayDirectionEXT, L, worldNrm);
        specular *= light.specularScale;
      }
    }
    else if(light.type == 2)  // Dome light 的环境高光
    {
      // 计算反射方向
      vec3 reflectDir = reflect(-gl_WorldRayDirectionEXT, worldNrm);

      // 将反射方向通过四元数旋转到dome空间
      vec3 domeUp = rotateByQuaternion(vec3(0, 1, 0), light.rotateQuat);

      // 基于反射方向与dome上方向的对齐程度计算高光
      float specAlignment = max(dot(reflectDir, domeUp), 0.0);

      if(specAlignment > 0)
      {
        // 使用幂函数控制高光的锐利程度
        float specIntensity = pow(specAlignment, mat.shininess * 0.25);  // 降低shininess影响
        specular            = mat.specular * specIntensity * light.specularScale;
      }
    }

    totalLight += lightEmission * shadowAttenuation * (diffuse + specular);
  }

  
  prd.hitValue = totalLight + emission;
  prd.objId    = int(gl_InstanceCustomIndexEXT);
  prd.instanceId = instanceIdBuf.instanceIds[gl_InstanceID];
}
