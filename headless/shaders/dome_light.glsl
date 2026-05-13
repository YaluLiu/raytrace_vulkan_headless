#ifndef ROBOT_DOME_LIGHT_GLSL
#define ROBOT_DOME_LIGHT_GLSL

const int LIGHT_TYPE_SPHERE = 0;
const int LIGHT_TYPE_DISTANT = 1;
const int LIGHT_TYPE_DOME = 2;

bool getFirstDomeLight(out Light domeLight)
{
  uint lightCount = min(frameUni.lightCount, MAX_SCENE_LIGHTS);
  for(uint i = 0; i < lightCount; ++i)
  {
    Light light = lights.i[i];
    if(light.type == LIGHT_TYPE_DOME)
    {
      domeLight = light;
      return true;
    }
  }
  return false;
}

vec3 rotateByQuat(vec4 quat, vec3 value)
{
  vec3 q = quat.xyz;
  return value + 2.0 * cross(q, cross(q, value) + quat.w * value);
}

vec4 inverseQuat(vec4 quat)
{
  return vec4(-quat.xyz, quat.w);
}

vec2 latLongUv(vec3 direction)
{
  vec3 d = normalize(direction);
  float u = atan(d.z, d.x) / (2.0 * PI) + 0.5;
  float v = acos(clamp(d.y, -1.0, 1.0)) / PI;
  return vec2(u, v);
}

vec3 sampleDomeLightLod(Light domeLight, vec3 worldDirection, float lod)
{
  vec3 localDirection = rotateByQuat(inverseQuat(domeLight.rotateQuat), normalize(worldDirection));
  vec3 domeColor = vec3(1.0);
  if(domeLight.textureID >= 0)
  {
    domeColor = textureLod(textureSamplers[nonuniformEXT(uint(domeLight.textureID))], latLongUv(localDirection), lod).rgb;
  }
  return domeColor * domeLight.baseEmission;
}

vec3 sampleDomeLight(Light domeLight, vec3 worldDirection)
{
  return sampleDomeLightLod(domeLight, worldDirection, 0.0);
}

vec3 sampleDomeLightDiffuse(Light domeLight, vec3 worldDirection)
{
  return sampleDomeLightLod(domeLight, worldDirection, 6.0);
}

#endif
