#ifndef COMMON_HOST_DEVICE
#define COMMON_HOST_DEVICE

#ifdef __cplusplus
#include <glm/glm.hpp>
using vec2 = glm::vec2;
using vec3 = glm::vec3;
using vec4 = glm::vec4;
using mat4 = glm::mat4;
using uint = unsigned int;
#endif

// clang-format off
#ifdef __cplusplus // Descriptor binding helper for C++ and GLSL
 #define START_BINDING(a) enum a {
 #define END_BINDING() }
#else
 #define START_BINDING(a)  const uint
 #define END_BINDING() 
#endif

START_BINDING(SceneBindings)
  eFrameUniforms = 0,  // Frame uniform containing the active camera
  eObjDescs    = 1,
  eTextures    = 2,
  eLights      = 3
END_BINDING();
// clang-format on

const uint MAX_SCENE_LIGHTS = 100;

struct ObjDesc
{
  int txtOffset;
  uint64_t vertexAddress;
  uint64_t indexAddress;
  uint64_t materialAddress;
  uint64_t materialIndexAddress;
};

struct PushConstantRaster
{
  mat4 model;
  uint objIndex;
  int instanceId;
  uint pad0;
  uint pad1;
};

struct CameraUniforms
{
  mat4 viewProj;
  mat4 view;
  mat4 viewInverse;
  mat4 projInverse;
  mat4 prevViewProj;  // Previous frame camera view * projection
};

struct FrameUniforms
{
  CameraUniforms camera;
  uint lightCount;
  uint pad0;
  uint pad1;
  uint pad2;
};

struct Light
{
  int type;
  int textureID;
  vec3 baseEmission;  // intensity * color * colorTemp * exposure
  float diffuse;
  float specular;
  vec3 direction;
  float angle;
  vec3 position;
  float radius;
  vec4 rotateQuat;
  vec3 padding;
};

struct Vertex
{
  vec3 pos;
  vec3 nrm;
  vec3 color;
  vec2 texCoord;
  vec4 tangent;
};

struct WaveFrontMaterial
{
  vec3 ambient;
  vec3 diffuse;
  vec3 specular;
  vec3 transmittance;
  vec3 emission;
  vec3 baseColorFactor;
  vec3 emissionFactor;
  vec3 transmissionColorFactor;
  vec3 subsurfaceColorFactor;
  float shininess;
  float ior;
  float opaque;  // 1 == opaque; 0 == fully transparent
  float metallicFactor;
  float roughnessFactor;
  float opacityFactor;
  float transmissionFactor;
  float subsurfaceFactor;
  float subsurfaceScale;
  int illum;
  int diffuseTextureId;
  int baseColorTextureId;
  int metallicTextureId;
  int roughnessTextureId;
  int normalTextureId;
  int emissionTextureId;
  int opacityTextureId;
  int subsurfaceTextureId;
};

const float PI = 3.1415926535897932384626433832795;

#endif
