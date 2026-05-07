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

vec4 sampleTextureRgba(int textureId, vec2 texCoord, vec4 fallbackValue)
{
  if(textureId < 0)
  {
    return fallbackValue;
  }
  return texture(textureSamplers[nonuniformEXT(uint(textureId))], texCoord);
}

PbrMaterialSample samplePbrMaterial(WaveFrontMaterial mat, vec2 texCoord)
{
  PbrMaterialSample pbr;

  vec3 baseColor = clamp(mat.baseColorFactor, vec3(0.0), vec3(1.0));
  if(mat.baseColorTextureId >= 0)
  {
    baseColor = clamp(mat.baseColorFactor * sampleTextureRgba(mat.baseColorTextureId, texCoord, vec4(1.0)).rgb,
                      vec3(0.0), vec3(1.0));
  }
  else if(mat.diffuseTextureId >= 0)
  {
    // Legacy OBJ/Hydra diffuse texture fallback.
    baseColor = clamp(baseColor * sampleTextureRgba(mat.diffuseTextureId, texCoord, vec4(1.0)).rgb, vec3(0.0), vec3(1.0));
  }

  float metallic = clamp(mat.metallicFactor, 0.0, 1.0);
  if(mat.metallicTextureId >= 0)
  {
    metallic = clamp(sampleTextureRgba(mat.metallicTextureId, texCoord, vec4(metallic)).r, 0.0, 1.0);
  }

  float roughness = clamp(mat.roughnessFactor, 0.02, 1.0);
  if(mat.roughnessTextureId >= 0)
  {
    roughness = clamp(sampleTextureRgba(mat.roughnessTextureId, texCoord, vec4(roughness)).r, 0.02, 1.0);
  }

  float transmission = clamp(mat.transmissionFactor, 0.0, 1.0);
  if(transmission > 0.0)
  {
    baseColor = clamp(mix(baseColor, mat.transmissionColorFactor, transmission * 0.75), vec3(0.0), vec3(1.0));
    metallic  = 0.0;
    roughness = clamp(min(roughness, mix(roughness, 0.08, transmission)), 0.02, 1.0);
  }

  float subsurface = clamp(mat.subsurfaceFactor, 0.0, 1.0);
  if(mat.subsurfaceTextureId >= 0)
  {
    subsurface *= clamp(sampleTextureRgba(mat.subsurfaceTextureId, texCoord, vec4(1.0)).r, 0.0, 1.0);
  }

  vec3 emission = mat.emissionFactor;
  if(mat.emissionTextureId >= 0)
  {
    emission *= sampleTextureRgba(mat.emissionTextureId, texCoord, vec4(1.0)).rgb;
  }

  float opacity = clamp(mat.opacityFactor, 0.0, 1.0);
  if(mat.opacityTextureId >= 0)
  {
    opacity *= clamp(sampleTextureRgba(mat.opacityTextureId, texCoord, vec4(1.0)).r, 0.0, 1.0);
  }

  pbr.baseColor     = baseColor;
  pbr.metallic      = metallic;
  pbr.roughness     = roughness;
  pbr.opacity       = opacity;
  pbr.transmission  = transmission;
  pbr.subsurface    = subsurface;
  pbr.subsurfaceScale = mat.subsurfaceScale;
  pbr.transmissionColor = clamp(mat.transmissionColorFactor, vec3(0.0), vec3(1.0));
  pbr.subsurfaceColor = clamp(mat.subsurfaceColorFactor, vec3(0.0), vec3(1.0));
  pbr.diffuseAlbedo = baseColor * (1.0 - metallic) * (1.0 - transmission * 0.35);
  pbr.specularF0    = clamp(mix(computeMetallicRoughnessSpecularF0(baseColor, metallic), vec3(0.08), transmission),
                           vec3(0.0), vec3(0.98));
  pbr.emission      = emission * opacity;
  return pbr;
}

vec3 sampleNormalMap(WaveFrontMaterial mat, vec2 texCoord, vec3 worldNrm, vec4 tangentW)
{
  if(mat.normalTextureId < 0)
  {
    return worldNrm;
  }

  vec3 normalSample = sampleTextureRgba(mat.normalTextureId, texCoord, vec4(0.5, 0.5, 1.0, 1.0)).xyz;
  normalSample      = normalSample * 2.0 - 1.0;

  vec3 tangent   = normalize(tangentW.xyz);
  tangent        = normalize(tangent - worldNrm * dot(worldNrm, tangent));
  vec3 bitangent = normalize(cross(worldNrm, tangent) * tangentW.w);
  mat3 tbn       = mat3(tangent, bitangent, worldNrm);
  return normalize(tbn * normalSample);
}

vec3 computePbrSceneLighting(vec3 worldPos, vec3 worldNrm, vec3 worldGeoNrm, PbrMaterialSample pbr)
{
  vec3 totalLight = vec3(0.0);
  int  numLights  = pcRay.numLights;

  for(int i = 0; i < numLights; i++)
  {
    Light light = lightBuf.lights[i];

    if(light.type == 2)  // Dome light
    {
      totalLight += pbr.diffuseAlbedo * light.baseEmission * light.diffuse;
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

    float shadowBias = computeShadowBias(worldPos);
    float tMin       = 0.001;
    float tMax       = max(lightDistance - shadowBias, tMin);
    vec3  origin     = worldPos + worldGeoNrm * shadowBias;
    uint  flags      = gl_RayFlagsTerminateOnFirstHitEXT | gl_RayFlagsOpaqueEXT | gl_RayFlagsSkipClosestHitShaderEXT;
    isShadowed       = true;
    traceRayEXT(topLevelAS, flags, 0xFF, 0, 0, 1, origin, tMin, L, tMax, 1);

    if(!isShadowed)
    {
      vec3 brdf = computePbrDirectLighting(pbr, gl_WorldRayDirectionEXT, L, worldNrm);
      totalLight += lightEmission * brdf * max(light.diffuse, light.specular);
      if(pbr.subsurface > 0.0)
      {
        float wrap = computeSubsurfaceWrap(pbr.subsurface, pbr.subsurfaceScale);
        float wrappedDotNL = clamp((dot(worldNrm, L) + wrap) / (1.0 + wrap), 0.0, 1.0);
        vec3 wrapDiffuse = pbr.diffuseAlbedo * pbr.subsurfaceColor * wrappedDotNL / PI;
        totalLight += lightEmission * wrapDiffuse * pbr.subsurface * light.diffuse;
      }
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
  recordSpecularHitDistance(prd, gl_HitTEXT);

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

  vec2 texCoord = v0.texCoord * barycentrics.x + v1.texCoord * barycentrics.y + v2.texCoord * barycentrics.z;
  vec4 tangentObj = v0.tangent * barycentrics.x + v1.tangent * barycentrics.y + v2.tangent * barycentrics.z;
  mat3 objectToWorldNormal = mat3(gl_ObjectToWorldEXT);
  vec4 tangentW = vec4(objectToWorldNormal * tangentObj.xyz, tangentObj.w);
  worldNrm = sampleNormalMap(mat, texCoord, worldNrm, tangentW);
  worldNrm = faceforward(worldNrm, gl_WorldRayDirectionEXT, worldGeoNrm);
  PbrMaterialSample pbr = samplePbrMaterial(mat, texCoord);

  if(prd.depth == 0)
  {
    vec3  diffuseAlbedo = clamp(pbr.diffuseAlbedo, vec3(0.0), vec3(1.0));
    vec3  specularF0    = pbr.specularF0;
    float roughnessSq   = pbr.roughness * pbr.roughness;

    prd.objId                   = int(gl_InstanceCustomIndexEXT);
    prd.instanceId              = instanceIdBuf.instanceIds[gl_InstanceID];
    prd.firstHitWorldPosRoughness = vec4(worldPos, roughnessSq);
    prd.firstHitNormalSpecHitDist = vec4(normalize(worldNrm), 0.0);
    prd.firstHitDiffuseValid      = vec4(diffuseAlbedo, 1.0);
    prd.firstHitSpecularPad       = vec4(specularF0, max(specularF0.x, max(specularF0.y, specularF0.z)) > 1e-4 ? 1.0 : 0.0);
  }

  vec3 emission = pbr.emission;
  vec3 direct   = computePbrSceneLighting(worldPos, worldNrm, worldGeoNrm, pbr);
  prd.radiance += prd.throughput * (emission + direct);

  int maxDepth  = max(pcRay.maxDepth, 1);
  int nextDepth = prd.depth + 1;
  if(nextDepth >= maxDepth)
  {
    prd.done = 1;
    return;
  }

  vec3 albedo = clamp(pbr.diffuseAlbedo, vec3(0.0), vec3(0.95));
  vec3 pathSpecularF0 = pbr.specularF0;
  bool firstSpecularBounce = prd.depth == 0 && max(pathSpecularF0.x, max(pathSpecularF0.y, pathSpecularF0.z)) > 1e-4;
  if(!firstSpecularBounce && max(albedo.x, max(albedo.y, albedo.z)) <= 1e-4)
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
  vec3  diffuseDir = normalize(localDir.x * tangent + localDir.y * bitangent + localDir.z * worldNrm);
  float roughness = pbr.roughness;
  vec3  bounceDir = chooseDlssFirstBounceDirection(gl_WorldRayDirectionEXT, worldNrm, diffuseDir, roughness, firstSpecularBounce);

  float cosTheta = max(dot(worldNrm, bounceDir), 0.0);
  if(cosTheta <= 0.0)
  {
    prd.done = 1;
    return;
  }

  prd.throughput *= firstSpecularBounce ? max(pathSpecularF0, vec3(0.04)) : albedo;

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
