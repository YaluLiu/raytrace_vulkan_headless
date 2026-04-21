#pragma once

#include "gl_vkpp.hpp"
#include "dlss/dlss_rr.hpp"

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
  void     initRayTracing();
  auto     objectToVkGeometryKHR(const ObjModel& model);
  void     createBottomLevelAS();
  void     createTopLevelAS();
  void     createRtDescriptorSet();
  void     updateRtDescriptorSet();
  void     createRtPipeline();
  void     raytrace(const VkCommandBuffer& cmdBuf);
  void     resetAccumulation();
  uint32_t getAccumulatedFrames() const { return m_accumulatedFrames; }

  VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
  nvvk::RaytracingBuilderKHR                        m_rtBuilder;
  nvvk::DescriptorSetBindings                       m_rtDescSetLayoutBind;
  VkDescriptorPool                                  m_rtDescPool{VK_NULL_HANDLE};
  VkDescriptorSetLayout                             m_rtDescSetLayout{VK_NULL_HANDLE};
  VkDescriptorSet                                   m_rtDescSet{VK_NULL_HANDLE};
  std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_rtShaderGroups;
  VkPipelineLayout                                  m_rtPipelineLayout{VK_NULL_HANDLE};
  VkPipeline                                        m_rtPipeline{VK_NULL_HANDLE};

  //for sbt
  nvvk::SBTWrapper                m_sbtWrapper;
  nvvk::Buffer                    m_rtSBTBuffer;
  VkStridedDeviceAddressRegionKHR m_rgenRegion{};
  VkStridedDeviceAddressRegionKHR m_missRegion{};
  VkStridedDeviceAddressRegionKHR m_hitRegion{};
  VkStridedDeviceAddressRegionKHR m_callRegion{};

  std::vector<VkAccelerationStructureInstanceKHR>    m_tlas;
  std::vector<nvvk::RaytracingBuilderKHR::BlasInput> m_blas;

  // Push constant for ray tracer
  PushConstantRay m_pcRay{
      0,  // num lights
      0,  // frame index
      6,  // max depth
      2,  // samples per frame
      {-90.0f, 90.0f, 0.5f, 3.0f, -2.0f, -20.0f, 1.0f, 361}};

  // #VK_animation
  void animationInstances(float time);
  void animationObject(float time);

  // #VK_compute
  void createCompDescriptors();
  void updateCompDescriptors(nvvk::Buffer& vertex);
  void createCompPipelines();

  nvvk::DescriptorSetBindings m_compDescSetLayoutBind;
  VkDescriptorPool            m_compDescPool{VK_NULL_HANDLE};
  VkDescriptorSetLayout       m_compDescSetLayout{VK_NULL_HANDLE};
  VkDescriptorSet             m_compDescSet{VK_NULL_HANDLE};
  VkPipeline                  m_compPipeline{VK_NULL_HANDLE};
  VkPipelineLayout            m_compPipelineLayout{VK_NULL_HANDLE};

  VkBuildAccelerationStructureFlagsKHR m_rtFlags;

  void saveOffscreenColorToFile(const char* filename);

  // DLSS-RR
  void createDlssRR();
  void runDlssRR(const VkCommandBuffer& cmdBuf);
  void setDlssRREnabled(bool enabled)
  {
    if(m_enableDlssRR != enabled)
    {
      m_enableDlssRR = enabled;
      if(m_enableDlssRR)
      {
        if(!m_dlssRR.isOperational())
        {
          createDlssRR();
        }
      }
      else
      {
        m_dlssRR.shutdown();
      }
      refreshOffscreenRenderTargetsIfNeeded();
      resetAccumulation();
    }
  }
  bool isDlssRREnabled() const { return m_enableDlssRR; }
  void setDlssSREnabled(bool enabled)
  {
    if(m_enableDlssSR != enabled)
    {
      m_enableDlssSR = enabled;
      refreshOffscreenRenderTargetsIfNeeded();
      resetAccumulation();
    }
  }
  bool isDlssSREnabled() const { return m_enableDlssSR; }
  void setDlssSRScale(float scale);
  float getDlssSRScale() const { return m_dlssSRScale; }
  void setSamplesPerFrame(int spp)
  {
    if(spp < 1)
    {
      spp = 1;
    }
    else if(spp > 64)
    {
      spp = 64;
    }

    if(m_pcRay.samplesPerFrame != spp)
    {
      m_pcRay.samplesPerFrame = spp;
      resetAccumulation();
    }
  }
  int getSamplesPerFrame() const { return m_pcRay.samplesPerFrame; }


  // #Post - Draw the rendered image on a quad using a tonemapper
  void createOffscreenRender();

  // color
  nvvk::Texture          m_offscreenColor;
  VkFormat               m_offscreenColorFormat{VK_FORMAT_R32G32B32A32_SFLOAT};
  nvvk::Texture          m_offscreenDlssOutput;
  VkFormat               m_offscreenDlssOutputFormat{VK_FORMAT_R32G32B32A32_SFLOAT};
  nvvk::Texture          m_offscreenDenoised;
  VkFormat               m_offscreenDenoisedFormat{VK_FORMAT_R32G32B32A32_SFLOAT};
  interop::Texture2DVkGL m_rtOutputGL;

  // primId
  nvvk::Texture          m_offscreenObjectId;  // VK_FORMAT_R32_SINT GL_R32I
  VkFormat               m_offscreenObjectIdFormat{VK_FORMAT_R32_SINT};
  interop::Texture2DVkGL m_rtObjectIdGL;

  // instanceId
  nvvk::Texture          m_offscreenInstanceId;
  VkFormat               m_offscreenInstanceIdFormat{VK_FORMAT_R32_SINT};
  interop::Texture2DVkGL m_rtInstanceIdGL;

  // DLSS-RR input buffers
  nvvk::Texture          m_offscreenDiffuseAlbedo;
  VkFormat               m_offscreenDiffuseAlbedoFormat{VK_FORMAT_R32G32B32A32_SFLOAT};
  interop::Texture2DVkGL m_rtDiffuseAlbedoGL;

  nvvk::Texture          m_offscreenSpecularAlbedo;
  VkFormat               m_offscreenSpecularAlbedoFormat{VK_FORMAT_R32G32B32A32_SFLOAT};
  interop::Texture2DVkGL m_rtSpecularAlbedoGL;

  nvvk::Texture          m_offscreenNormalRoughness;
  VkFormat               m_offscreenNormalRoughnessFormat{VK_FORMAT_R32G32B32A32_SFLOAT};
  interop::Texture2DVkGL m_rtNormalRoughnessGL;

  nvvk::Texture          m_offscreenMotionVector;
  VkFormat               m_offscreenMotionVectorFormat{VK_FORMAT_R32G32_SFLOAT};
  interop::Texture2DVkGL m_rtMotionVectorGL;

  nvvk::Texture          m_offscreenLinearDepth;
  VkFormat               m_offscreenLinearDepthFormat{VK_FORMAT_R32_SFLOAT};
  interop::Texture2DVkGL m_rtLinearDepthGL;

  nvvk::Texture          m_offscreenSpecularHitDistance;
  VkFormat               m_offscreenSpecularHitDistanceFormat{VK_FORMAT_R32_SFLOAT};
  interop::Texture2DVkGL m_rtSpecularHitDistanceGL;

  nvvk::Texture          m_offscreenDistanceToCamera;
  VkFormat               m_offscreenDistanceToCameraFormat{VK_FORMAT_R32_SFLOAT};
  interop::Texture2DVkGL m_rtDistanceToCameraGL;

  nvvk::Texture          m_offscreenLidarPointCloud;
  VkFormat               m_offscreenLidarPointCloudFormat{VK_FORMAT_R32G32B32A32_SFLOAT};
  interop::Texture2DVkGL m_rtLidarPointCloudGL;

  interop::ResourceAllocatorGLInterop m_allocGL;
  dlss::DlssRR                        m_dlssRR;
  bool                                m_enableDlssRR{true};
  bool                                m_enableDlssSR{true};
  float                               m_dlssSRScale{0.6f};
  VkExtent2D                          m_renderSize{0, 0};

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
  std::vector<Light> m_uploadedLights;
  nvvk::Buffer       m_bInstanceIds;
  nvvk::Buffer       m_bLights;

  void addLight(const Light& light);
  void clearLights();
  void createLightBuffer();
  void updateLightBuffer(const VkCommandBuffer& cmdBuf);
  void createInstanceIdBuffer();
  void updateInstanceIdBuffer(const VkCommandBuffer& cmdBuf);

  // for sphere cloud
  std::vector<Sphere> m_spheres;                // All spheres
  nvvk::Buffer        m_spheresBuffer;          // Buffer holding the spheres
  nvvk::Buffer        m_spheresAabbBuffer;      // Buffer of all Aabb
  nvvk::Buffer        m_spheresMatColorBuffer;  // Multiple materials
  nvvk::Buffer        m_spheresMatIndexBuffer;  // Define which sphere uses which material

  void createSpheres(uint32_t nbSpheres);
  void addSpheres(std::vector<Sphere> vector);
  auto sphereToVkGeometryKHR();
  void createRtShaderBindingTable();

private:
  void createOffscreenImage(nvvk::Texture&          texture,
                            VkFormat                format,
                            VkImageUsageFlags       usage,
                            interop::Texture2DVkGL* interopTexture,
                            int                     glInternalFormat,
                            int                     glMinFilter,
                            int                     glMagFilter,
                            VkExtent2D              extent = {0, 0});
  bool       shouldRenderAtDlssScale() const;
  VkExtent2D computeRenderSize() const;
  void       refreshOffscreenRenderTargetsIfNeeded();

  uint32_t  m_accumulatedFrames{0};
  glm::mat4 m_lastView{1.0f};
  glm::mat4 m_lastProj{1.0f};
  bool      m_hasLastCamera{false};
};
