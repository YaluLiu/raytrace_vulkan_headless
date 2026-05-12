struct hitPayload
{
  vec3 radiance;
  int  objId;
  int  instanceId;
  vec3 throughput;
  uint seed;
  int  depth;
  int  done;
  vec4 firstHitWorldPosRoughness;  // xyz = world position, w = alpha roughness^2
  vec4 firstHitNormalSpecHitDist;  // xyz = world normal, w = reserved
  vec4 firstHitDiffuseValid;       // xyz = diffuse albedo, w = valid flag
  vec4 firstHitSpecularPad;        // xyz = specular F0, w = has specular lobe
};

uint initRng(uvec2 pixel, uint frameIndex, uint sampleIndex)
{
  uint seed = pixel.x * 1973u + pixel.y * 9277u + frameIndex * 26699u + sampleIndex * 31817u + 1u;
  seed ^= seed << 13;
  seed ^= seed >> 17;
  seed ^= seed << 5;
  return seed;
}

float rand01(inout uint seed)
{
  seed = 1664525u * seed + 1013904223u;
  return float(seed & 0x00FFFFFFu) / float(0x01000000u);
}

vec3 cosineSampleHemisphere(float u1, float u2)
{
  const float kPi = 3.14159265358979323846;
  float r   = sqrt(u1);
  float phi = 2.0 * kPi * u2;
  return vec3(r * cos(phi), r * sin(phi), sqrt(max(0.0, 1.0 - u1)));
}

void orthonormalBasis(vec3 n, out vec3 t, out vec3 b)
{
  if(abs(n.z) < 0.999)
  {
    t = normalize(cross(vec3(0.0, 0.0, 1.0), n));
  }
  else
  {
    t = normalize(cross(vec3(0.0, 1.0, 0.0), n));
  }
  b = cross(n, t);
}

vec3 chooseFirstBounceDirection(vec3 incomingDir, vec3 normal, vec3 diffuseDir, float roughness, bool hasSpecularLobe)
{
  if(!hasSpecularLobe)
  {
    return diffuseDir;
  }

  vec3 reflectionDir = normalize(reflect(incomingDir, normal));
  if(dot(normal, reflectionDir) <= 0.0)
  {
    return diffuseDir;
  }

  vec3 glossyDir = normalize(mix(reflectionDir, diffuseDir, clamp(roughness, 0.0, 1.0)));
  return (dot(normal, glossyDir) > 0.0) ? glossyDir : reflectionDir;
}
