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

float computeShadowBias(vec3 worldPos)
{
  float sceneScale = max(max(abs(worldPos.x), abs(worldPos.y)), abs(worldPos.z));
  return max(0.001, sceneScale * 0.0001);
}

vec3 computeDirectLighting(vec3 worldPos, vec3 worldNrm, vec3 worldGeoNrm, WaveFrontMaterial mat, vec3 textureColor)
{
  vec3 totalLight = vec3(0.0);
  int  numLights  = pcRay.numLights;

  for(int i = 0; i < numLights; i++)
  {
    Light light = lightBuf.lights[i];

    if(light.type == 2)  // Dome light
    {
      totalLight += mat.diffuse * textureColor * light.baseEmission * light.diffuse;
      continue;
    }

    vec3  L;
    float lightDistance       = 100000.0;
    float distanceAttenuation = 1.0;
    vec3  lightEmission       = vec3(0.0);

    if(light.type == 0)  // Sphere light
    {
      vec3 lDir     = light.position.xyz - worldPos;
      lightDistance = length(lDir);
      float effectiveDistance = max(lightDistance - light.radius, max(light.radius * 0.25, 0.05));
      distanceAttenuation     = 1.0 / (effectiveDistance * effectiveDistance);

      L             = normalize(lDir);
      lightEmission = light.baseEmission * distanceAttenuation;
    }
    else if(light.type == 1)  // Distant light
    {
      L                   = normalize(light.direction.xyz);
      distanceAttenuation = light.angle;
      lightEmission       = light.baseEmission * distanceAttenuation;
    }
    else
    {
      continue;
    }

    if(dot(worldNrm, L) <= 0.0)
    {
      continue;
    }

    vec3 diffuse = computeDiffuse(mat, L, worldNrm);
    diffuse *= textureColor * light.diffuse;

    vec3 specular = vec3(0.0);

    float shadowBias = computeShadowBias(worldPos);
    float tMin       = 0.001;
    float tMax       = max(lightDistance - shadowBias, tMin);
    vec3  origin     = worldPos + worldGeoNrm * shadowBias;
    uint  flags      = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
    isShadowed       = true;
    traceRayEXT(topLevelAS, flags, 0xFF, 0, 0, 1, origin, tMin, L, tMax, 1);

    if(!isShadowed)
    {
      specular = computeSpecular(mat, gl_WorldRayDirectionEXT, L, worldNrm);
      specular *= light.specular;
      totalLight += lightEmission * (diffuse + specular);
    }
  }

  return totalLight;
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

  const mat3 normalMatrix    = transpose(mat3(gl_WorldToObjectEXT));
  const vec3 geometricNrm    = cross(v1.pos - v0.pos, v2.pos - v0.pos);
  const vec3 interpolatedNrm = v0.nrm * barycentrics.x + v1.nrm * barycentrics.y + v2.nrm * barycentrics.z;
  vec3       worldGeoNrm     = normalize(normalMatrix * geometricNrm);
  vec3       worldShadeNrm   = normalize(normalMatrix * interpolatedNrm);
  if(length(worldShadeNrm) < 1e-5)
  {
    worldShadeNrm = worldGeoNrm;
  }

  worldGeoNrm   = faceforward(worldGeoNrm, gl_WorldRayDirectionEXT, worldGeoNrm);
  vec3 worldNrm = faceforward(worldShadeNrm, gl_WorldRayDirectionEXT, worldGeoNrm);

  int               matIdx = matIndices.i[gl_PrimitiveID];
  WaveFrontMaterial mat    = materials.m[matIdx];

  vec3 textureColor = vec3(1.0);
  if(mat.textureId >= 0)
  {
    uint txtId    = mat.textureId;
    vec2 texCoord = v0.texCoord * barycentrics.x + v1.texCoord * barycentrics.y + v2.texCoord * barycentrics.z;
    textureColor  = texture(textureSamplers[nonuniformEXT(txtId)], texCoord).xyz;
  }

  if(prd.depth == 0)
  {
    prd.objId      = int(gl_InstanceCustomIndexEXT);
    prd.instanceId = instanceIdBuf.instanceIds[gl_InstanceID];
  }

  vec3 emission = computeEmission(mat, textureColor);
  vec3 direct   = computeDirectLighting(worldPos, worldNrm, worldGeoNrm, mat, textureColor);
  prd.radiance += prd.throughput * (emission + direct);

  int maxDepth  = max(pcRay.maxDepth, 1);
  int nextDepth = prd.depth + 1;
  if(nextDepth >= maxDepth)
  {
    prd.done = 1;
    return;
  }

  vec3 albedo = clamp(mat.diffuse * textureColor, vec3(0.0), vec3(0.95));
  if(max(albedo.x, max(albedo.y, albedo.z)) <= 1e-4)
  {
    prd.done = 1;
    return;
  }

  vec3 tangent;
  vec3 bitangent;
  orthonormalBasis(worldNrm, tangent, bitangent);

  float u1 = rand01(prd.seed);
  float u2 = rand01(prd.seed);
  vec3  localDir  = cosineSampleHemisphere(u1, u2);
  vec3  bounceDir = normalize(localDir.x * tangent + localDir.y * bitangent + localDir.z * worldNrm);

  float cosTheta = max(dot(worldNrm, bounceDir), 0.0);
  if(cosTheta <= 0.0)
  {
    prd.done = 1;
    return;
  }

  prd.throughput *= albedo;

  if(nextDepth >= 2)
  {
    float rrProb = clamp(max(prd.throughput.x, max(prd.throughput.y, prd.throughput.z)), 0.05, 0.95);
    if(rand01(prd.seed) > rrProb)
    {
      prd.done = 1;
      return;
    }
    prd.throughput /= rrProb;
  }

  prd.depth = nextDepth;

  float shadowBias = computeShadowBias(worldPos);
  vec3  origin     = worldPos + worldGeoNrm * shadowBias;
  traceRayEXT(topLevelAS, gl_RayFlagsOpaqueEXT, 0xFF, 0, 0, 0, origin, 0.001, bounceDir, 10000.0, 0);
}
