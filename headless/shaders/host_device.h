#ifndef COMMON_HOST_DEVICE
#define COMMON_HOST_DEVICE

#ifdef __cplusplus
#include <glm/glm.hpp>
// GLSL Type
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
  eGlobals     = 0,  // Global uniform containing camera matrices
  eObjDescs    = 1,  // Access to the object descriptions
  eTextures    = 2,  // Access to textures
  eLights      = 3,  // 灯光缓冲区
  eInstanceIds = 4,   // 新增：实例ID缓冲区
  eImplicit    = 5
END_BINDING();


START_BINDING(RtxBindings)
  eTlas                   = 0,  // Top-level acceleration structure
  eOutImage               = 1,  // Ray tracer output image (color)
  eObjIdImage             = 2,  // ObjectId output
  eInsIdImage             = 3,  // InstanceId output
  eDiffuseAlbedoImage     = 4,  // DLSS-RR: diffuse albedo input
  eSpecularAlbedoImage    = 5,  // DLSS-RR: specular albedo input
  eNormalRoughnessImage   = 6,  // DLSS-RR: packed normal+roughness
  eMotionVectorImage      = 7,  // DLSS-RR: motion vector
  eLinearDepthImage       = 8,  // DLSS-RR: linear depth
  eSpecularHitDistImage   = 9,  // DLSS-RR: specular hit distance (optional)
  eDistanceToCameraImage  = 10, // AOV: world-space distance from camera to first hit
  eLidarPointCloudImage   = 11  // AOV: lidar point cloud
END_BINDING();
// clang-format on

// Information of a obj model when referenced in a shader
struct ObjDesc
{
  int      txtOffset;             // Texture index offset in the array of textures
  uint64_t vertexAddress;         // Address of the Vertex buffer
  uint64_t indexAddress;          // Address of the index buffer
  uint64_t materialAddress;       // Address of the material buffer
  uint64_t materialIndexAddress;  // Address of the triangle material index buffer
};

// Uniform buffer set at each frame
struct GlobalUniforms
{
  mat4 viewProj;      // Camera view * projection
  mat4 view;          // Camera view matrix
  mat4 viewInverse;   // Camera inverse view matrix
  mat4 projInverse;   // Camera inverse projection matrix
  mat4 prevViewProj;  // Previous frame camera view * projection
};

struct Light
{
  // common
  int   type;
  int   textureID;
  vec3  baseEmission;  // intensity * color * colorTemp * exposure
  float diffuse;
  float specular;
  // distant light
  vec3  direction;
  float angle;
  // sphere light
  vec3  position;
  float radius;
  // dome light
  vec4 rotateQuat;  // padding for alignment
  vec3 padding;
};


// 修改 PushConstantRay 结构体：
struct PushConstantRay
{
  vec4  clearColor;
  vec3  lightPosition;
  float lightIntensity;
  int   lightType;
  int   numLights;
  uint  frameIndex;
  int   maxDepth;
  int   samplesPerFrame;
};

struct Vertex  // See ObjLoader, copy of VertexObj, could be compressed for device
{
  vec3 pos;
  vec3 nrm;
  vec3 color;
  vec2 texCoord;
};

struct WaveFrontMaterial  // See ObjLoader, copy of MaterialObj, could be compressed for device
{
  vec3  ambient;
  vec3  diffuse;
  vec3  specular;
  vec3  transmittance;
  vec3  emission;
  float shininess;
  float ior;       // index of refraction
  float dissolve;  // 1 == opaque; 0 == fully transparent
  int   illum;     // illumination model (see http://www.fileformat.info/format/material/)
  int   textureId;
};

struct Sphere
{
  vec3  center;
  float radius;
};

struct Aabb
{
  vec3 minimum;
  vec3 maximum;
};

const float PI = 3.1415926535897932384626433832795;

#define KIND_SPHERE 0
#define KIND_CUBE 1


#endif
