#pragma once

#include "gl_vkpp.hpp"

#include "headless_vk.hpp"
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/descriptorsets_vk.hpp"
#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include "shaders/host_device.h"

// #VKRay
#include "nvvk/raytraceKHR_vk.hpp"
#include "nvvk/sbtwrapper_vk.hpp"

#include "ModelLoader.h"

struct MaterialUpdate
{
  int               modelIndex;
  int               materialIndex;
  WaveFrontMaterial newMaterial;
};

//--------------------------------------------------------------------------------------------------
// Simple rasterizer of OBJ objects
// - Each OBJ loaded are stored in an `ObjModel` and referenced by a `ObjInstance`
// - It is possible to have many `ObjInstance` referencing the same `ObjModel`
// - Rendering is done in an offscreen framebuffer
// - The image of the framebuffer is displayed in post-process in a full-screen quad
//
class HelloVulkan : public nvvkhl::AppOffline
{
public:
  void setup(const VkInstance& instance, const VkDevice& device, const VkPhysicalDevice& physicalDevice, uint32_t queueFamily) override;
  void createDescriptorSetLayout();
  void loadModel(ModelLoader& loader, glm::mat4 transform = glm::mat4(1));
  void updateDescriptorSet();
  void createUniformBuffer();
  void createObjDescriptionBuffer();
  void createTextureImages(const VkCommandBuffer& cmdBuf, const std::vector<std::string>& textures);
  void updateUniformBuffer(const VkCommandBuffer& cmdBuf);
  void onResize(int /*w*/, int /*h*/);
  void destroyResources();

  // The OBJ model
  struct ObjModel
  {
    uint32_t     nbIndices{0};
    uint32_t     nbVertices{0};
    nvvk::Buffer vertexBuffer;    // Device buffer of all 'Vertex'
    nvvk::Buffer indexBuffer;     // Device buffer of the indices forming triangles
    nvvk::Buffer matColorBuffer;  // Device buffer of array of 'Wavefront material'
    nvvk::Buffer matIndexBuffer;  // Device buffer of array of 'Wavefront material'
  };

  struct ObjInstance
  {
    glm::mat4 transform;    // Matrix of the instance
    uint32_t  objIndex{0};  // Model index reference
  };

  // Array of objects and instances in the scene
  std::vector<ModelLoader> m_Loader;     // Model on host
  std::vector<ObjModel>    m_objModel;   // Model on host
  std::vector<ObjDesc>     m_objDesc;    // Model description for device access
  std::vector<ObjInstance> m_instances;  // Scene model instances

  // Graphic pipeline
  nvvk::DescriptorSetBindings m_descSetLayoutBind;
  VkDescriptorPool            m_descPool;
  VkDescriptorSetLayout       m_descSetLayout;
  VkDescriptorSet             m_descSet;

  nvvk::Buffer m_bGlobals;  // Device-Host of the camera matrices
  nvvk::Buffer m_bObjDesc;  // Device buffer of the OBJ descriptions

  std::vector<nvvk::Texture> m_textures;  // vector of all textures of the scene

  nvvk::ResourceAllocatorDma m_alloc;  // Allocator for buffer, images, acceleration structures
  nvvk::DebugUtil            m_debug;  // Utility to name objects

  // #VKRay
  void initRayTracing();
  auto objectToVkGeometryKHR(const ObjModel& model);
  void createBottomLevelAS();
  void createTopLevelAS();
  void createRtDescriptorSet();
  void updateRtDescriptorSet();
  void createRtPipeline();
  void raytrace(const VkCommandBuffer& cmdBuf);

  VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
  nvvk::RaytracingBuilderKHR                        m_rtBuilder;
  nvvk::DescriptorSetBindings                       m_rtDescSetLayoutBind;
  VkDescriptorPool                                  m_rtDescPool;
  VkDescriptorSetLayout                             m_rtDescSetLayout;
  VkDescriptorSet                                   m_rtDescSet;
  std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_rtShaderGroups;
  VkPipelineLayout                                  m_rtPipelineLayout;
  VkPipeline                                        m_rtPipeline;
  nvvk::SBTWrapper                                  m_sbtWrapper;

  std::vector<VkAccelerationStructureInstanceKHR>    m_tlas;
  std::vector<nvvk::RaytracingBuilderKHR::BlasInput> m_blas;

  // Push constant for ray tracer
  PushConstantRay m_pcRay{
      {1, 1, 1, 1.00f},  // clear color
      {5.f, 10.f, 5.f},  // light position
      100.f,             // light intensity
      0                  // light type
  };

  // #VK_animation
  void animationInstances(float time);
  void animationObject(float time);

  // #VK_compute
  void createCompDescriptors();
  void updateCompDescriptors(nvvk::Buffer& vertex);
  void createCompPipelines();

  nvvk::DescriptorSetBindings m_compDescSetLayoutBind;
  VkDescriptorPool            m_compDescPool;
  VkDescriptorSetLayout       m_compDescSetLayout;
  VkDescriptorSet             m_compDescSet;
  VkPipeline                  m_compPipeline;
  VkPipelineLayout            m_compPipelineLayout;

  VkBuildAccelerationStructureFlagsKHR m_rtFlags;

  void saveOffscreenColorToFile(const char* filename);


  // #Post - Draw the rendered image on a quad using a tonemapper
  void createOffscreenRender();

  // color
  nvvk::Texture          m_offscreenColor;
  VkFormat               m_offscreenColorFormat{VK_FORMAT_R32G32B32A32_SFLOAT};
  void                   createOutputImage();
  interop::Texture2DVkGL m_rtOutputGL;

  // primId
  nvvk::Texture          m_offscreenObjectId;  // VK_FORMAT_R32_SINT GL_R32I
  VkFormat               m_offscreenObjectIdFormat{VK_FORMAT_R32_SINT};
  void                   createObjectIdImage();
  interop::Texture2DVkGL m_rtObjectIdGL;

  // instanceId
  nvvk::Texture          m_offscreenInstanceId;
  VkFormat               m_offscreenInstanceIdFormat{VK_FORMAT_R32_SINT};
  void                   createInstanceIdImage();
  interop::Texture2DVkGL m_rtInstanceIdGL;

  interop::ResourceAllocatorGLInterop m_allocGL;

  // depth buffer
  nvvk::Texture m_offscreenDepth;
  VkFormat      m_offscreenDepthFormat{VK_FORMAT_X8_D24_UNORM_PACK32};

  std::vector<uint32_t> readObjectIdImage();
  void                  dumpInteropTexture(const char* filename);

  // RayTrace Stucture
  void updateTlas(uint32_t mesh_Id, glm::mat4 transform, bool visible);
  void updateTlasEnd();
  void updateBlas(uint32_t mesh_Id);
  void updateMaterialAtRuntime(int modelIndex, int materialIndex, const WaveFrontMaterial& newMaterial);
  void updateMaterialsAtRuntime(const std::vector<MaterialUpdate>& updates);

  // hydra plugin
  std::vector<Light> m_lights;       // for hydra Light
  std::vector<int>   m_instanceIds;  // for hydra store instance Ids
  nvvk::Buffer       m_bInstanceIds;
  nvvk::Buffer       m_bLights;

  void addLight(const Light& light);
  void clearLights();
  void createLightBuffer();
  void updateLightBuffer(const VkCommandBuffer& cmdBuf);
  void createInstanceIdBuffer();
  void updateInstanceIdBuffer(const VkCommandBuffer& cmdBuf);
};