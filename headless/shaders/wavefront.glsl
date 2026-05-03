#include "host_device.h"

vec3 computeDiffuse(WaveFrontMaterial mat, vec3 lightDir, vec3 normal)
{
  // Lambertian
  float dotNL = max(dot(normal, lightDir), 0.0);
  vec3  c     = mat.diffuse * dotNL;
  if(mat.illum >= 1)
    c += mat.ambient;
  return c;
}

vec3 computeSpecular(WaveFrontMaterial mat, vec3 viewDir, vec3 lightDir, vec3 normal)
{
  if(mat.illum < 2)
    return vec3(0);

  vec3 V = normalize(-viewDir);
  vec3 L = normalize(lightDir);
  vec3 H = normalize(V + L);

  float dotNL = max(dot(normal, L), 0.0);
  float dotNV = max(dot(normal, V), 0.0);
  float dotNH = max(dot(normal, H), 0.0);
  float dotVH = max(dot(V, H), 0.0);
  if(dotNL <= 0.0 || dotNV <= 0.0)
    return vec3(0);

  // Normalized Blinn-Phong gives a softer, more stable highlight than reflect-based Phong.
  const float kShininess = clamp(mat.shininess, 8.0, 256.0);
  const float kNorm      = (kShininess + 8.0) / (8.0 * PI);
  float       specular   = kNorm * pow(dotNH, kShininess) * dotNL;

  vec3 specularColor = clamp(mat.specular, vec3(0.0), vec3(0.98));
  vec3 fresnel       = specularColor + (vec3(1.0) - specularColor) * pow(1.0 - dotVH, 5.0);

  return fresnel * specular;
}

vec3 computeEmission(WaveFrontMaterial mat, vec3 textureColor)
{
  vec3 emission = mat.emission;
  emission *= mat.dissolve;
  emission *= textureColor;

  return emission;
}

struct PbrMaterialSample
{
  vec3  baseColor;
  vec3  diffuseAlbedo;
  vec3  specularF0;
  vec3  emission;
  float metallic;
  float roughness;
  float opacity;
};

vec3 computeMetallicRoughnessSpecularF0(vec3 baseColor, float metallic)
{
  return mix(vec3(0.04), baseColor, clamp(metallic, 0.0, 1.0));
}

float distributionGGX(float dotNH, float roughness)
{
  float alpha  = max(roughness * roughness, 0.001);
  float alpha2 = alpha * alpha;
  float denom  = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
  return alpha2 / max(PI * denom * denom, 1.0e-5);
}

float geometrySmithGGX(float dotNL, float dotNV, float roughness)
{
  float r = roughness + 1.0;
  float k = (r * r) / 8.0;
  float gl = dotNL / max(dotNL * (1.0 - k) + k, 1.0e-5);
  float gv = dotNV / max(dotNV * (1.0 - k) + k, 1.0e-5);
  return gl * gv;
}

vec3 fresnelSchlick(float dotVH, vec3 f0)
{
  return f0 + (vec3(1.0) - f0) * pow(clamp(1.0 - dotVH, 0.0, 1.0), 5.0);
}

vec3 computePbrDirectLighting(PbrMaterialSample pbr, vec3 viewDir, vec3 lightDir, vec3 normal)
{
  vec3 V = normalize(-viewDir);
  vec3 L = normalize(lightDir);
  vec3 H = normalize(V + L);

  float dotNL = max(dot(normal, L), 0.0);
  float dotNV = max(dot(normal, V), 0.0);
  float dotNH = max(dot(normal, H), 0.0);
  float dotVH = max(dot(V, H), 0.0);
  if(dotNL <= 0.0 || dotNV <= 0.0)
    return vec3(0.0);

  vec3  diffuse = pbr.diffuseAlbedo / PI;
  float d       = distributionGGX(dotNH, pbr.roughness);
  float g       = geometrySmithGGX(dotNL, dotNV, pbr.roughness);
  vec3  f       = fresnelSchlick(dotVH, pbr.specularF0);
  vec3  specular = (d * g * f) / max(4.0 * dotNL * dotNV, 1.0e-5);

  return (diffuse + specular) * dotNL;
}
