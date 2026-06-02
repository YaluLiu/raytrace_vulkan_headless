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
  eLights      = 3,
  eTileFrameUniforms = 4
END_BINDING();
// clang-format on

const uint MAX_SCENE_LIGHTS = 100;
const uint MAX_TILE_MULTIVIEW_VIEWS = 16;
const uint LIDAR_POINT_FLAG_VALID = 1u << 0;
const uint LIDAR_POINT_FLAG_HIT = 1u << 1;
const uint LIDAR_POINT_FLAG_OUT_OF_RANGE = 1u << 2;
const uint HEIGHT_SCAN_SAMPLE_FLAG_VALID = 1u << 0;
const uint HEIGHT_SCAN_SAMPLE_FLAG_HIT = 1u << 1;
const uint HEIGHT_SCAN_SAMPLE_FLAG_OUT_OF_RANGE = 1u << 2;
const uint TRACE_MASK_DEFAULT_GEOMETRY = 0x01u;
const uint TRACE_MASK_GROUND = 0x02u;

struct ObjDesc
{
  int txtOffset;
  uint64_t vertexAddress;
  uint64_t indexAddress;
  uint64_t materialAddress;
  uint64_t materialIndexAddress;
};

struct MeshDrawPushConstants
{
  mat4 model;
  uint objIndex;
  int instanceId;
  uint pad0;
  uint pad1;
};

struct LidarPointGpu
{
  vec4 positionRange;
  uint sensorIndex;
  uint ringIndex;
  uint beamIndex;
  uint flags;
  float intensity;
  uint pad0;
  uint pad1;
  uint pad2;
};

struct LidarSensorGpu
{
  vec4 originMaxRange;
  vec4 forwardAzimuthStart;
  vec4 rightAzimuthStep;
  vec4 upVerticalStart;
  vec4 azimuthEndVerticalEndStep;
  uint pointOffset;
  uint pointCount;
  uint azimuthSampleCount;
  uint verticalSampleCount;
  float intensity;
  uint pad0;
  uint pad1;
  uint pad2;
};

struct HeightScanSampleGpu
{
  vec4 positionDistance;
  uint sensorIndex;
  uint uIndex;
  uint vIndex;
  uint flags;
};

struct HeightScanSensorGpu
{
  vec4 originMaxRange;
  vec4 gravityDirectionWsUStart;
  vec4 axisUAndUEnd;
  vec4 axisVAndVStart;
  vec4 uStepVEndVStep;
  uint sampleOffset;
  uint sampleCount;
  uint uSampleCount;
  uint vSampleCount;
};

struct LidarFrameGpu
{
  uint sensorCount;
  uint totalPointCount;
  uint frameIdLow;
  uint frameIdHigh;
};

struct PushConstantLidarGenerate
{
  uint sensorIndex;
  uint pad0;
  uint pad1;
  uint pad2;
};

struct PushConstantHeightScanGenerate
{
  uint sensorIndex;
  uint pad0;
  uint pad1;
  uint pad2;
};

struct PushConstantPointOverlay
{
  uint sensorIndex;
  float pointSizePixels;
  uint pad0;
  uint pad1;
};

struct CameraUniforms
{
  mat4 viewProj;
  mat4 view;
  mat4 viewInverse;
  mat4 projInverse;
};

struct FrameUniforms
{
  CameraUniforms camera;
  uint lightCount;
  uint pad0;
  uint pad1;
  uint pad2;
};

struct TileFrameUniforms
{
  CameraUniforms cameras[MAX_TILE_MULTIVIEW_VIEWS];
  uint lightCount;
  uint viewCount;
  uint batchBaseCameraIndex;
  uint pad0;
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
