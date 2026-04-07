struct hitPayload
{
  vec3 radiance;
  int  objId;
  int  instanceId;
  vec3 throughput;
  uint seed;
  int  depth;
  int  done;
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
