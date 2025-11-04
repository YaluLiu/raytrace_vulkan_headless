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
layout(set = 1, binding = eLights, scalar) buffer LightBuf { Light lights[];} lightBuf;
layout(set = 1, binding = eInstanceIds, scalar) buffer InstanceIdBuf { int instanceIds[]; } instanceIdBuf;
layout(push_constant) uniform _PushConstantRay { PushConstantRay pcRay; };

vec3 rotateByQuaternion(vec3 v, vec4 q) 
{
  vec3 u = q.xyz;
  float s = q.w;
  return 2.0 * dot(u, v) * u + (s * s - dot(u, u)) * v + 2.0 * s * cross(u, v);
}

// 将世界空间方向转换为球面坐标 UV
vec2 directionToSphericalUV(vec3 dir)
{
  float phi = atan(dir.z, dir.x);
  float theta = acos(clamp(dir.y, -1.0, 1.0));
  
  float u = phi / (2.0 * 3.14159265359) + 0.5;
  float v = theta / 3.14159265359;
  
  return vec2(u, v);
}

void main()
{
  ObjDesc    objResource = objDesc.i[gl_InstanceCustomIndexEXT];
  MatIndices matIndices  = MatIndices(objResource.materialIndexAddress);
  Materials  materials   = Materials(objResource.materialAddress);
  Indices    indices     = Indices(objResource.indexAddress);
  Vertices   vertices    = Vertices(objResource.vertexAddress);

  ivec3 ind = indices.i[gl_PrimitiveID];

  Vertex v0 = vertices.v[ind.x];
  Vertex v1 = vertices.v[ind.y];
  Vertex v2 = vertices.v[ind.z];

  const vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

  const vec3 pos      = v0.pos * barycentrics.x + v1.pos * barycentrics.y + v2.pos * barycentrics.z;
  const vec3 worldPos = vec3(gl_ObjectToWorldEXT * vec4(pos, 1.0));  

  const vec3 nrm      = v0.nrm * barycentrics.x + v1.nrm * barycentrics.y + v2.nrm * barycentrics.z;
  const vec3 worldNrm = normalize(vec3(nrm * gl_WorldToObjectEXT));  // Transforming the normal to world space

  int               matIdx = matIndices.i[gl_PrimitiveID];
  WaveFrontMaterial mat    = materials.m[matIdx];

  vec3 textureColor = vec3(1.0);
  if(mat.textureId >= 0)
  {
    uint txtId    = mat.textureId;
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
    float distanceAttenuation = 1.0;
    vec3 lightEmission;

    if(light.type == 0)  // Sphere light
    {
      vec3 lDir     = light.position.xyz - worldPos;
      lightDistance = length(lDir);
      float effectiveDistance = max(lightDistance - light.radius, 0.001);
      distanceAttenuation     = 1.0 / (effectiveDistance * effectiveDistance);

      L = normalize(lDir);
      lightEmission = light.baseEmission * distanceAttenuation;
    }
    else if(light.type == 1)  // Distant light
    {
      L = normalize(light.direction.xyz);
      distanceAttenuation = light.angleScale;
      lightEmission = light.baseEmission * distanceAttenuation;
    }


    vec3 diffuse = computeDiffuse(mat, L, worldNrm);
    diffuse *= textureColor * light.diffuseScale;

    vec3  specular          = vec3(0);
    float shadowAttenuation = 1.0;

    if(dot(worldNrm, L) > 0)
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

    totalLight += lightEmission * shadowAttenuation * (diffuse + specular);
  }

  prd.hitValue = totalLight + emission;
  prd.objId    = int(gl_InstanceCustomIndexEXT);
  prd.instanceId = instanceIdBuf.instanceIds[gl_InstanceID];
}
