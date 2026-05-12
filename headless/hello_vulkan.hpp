#pragma once

#include "aov_texture.hpp"

#include "headless_vk.hpp"
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/descriptorsets_vk.hpp"
#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include "shaders/host_device.h"

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
  struct RadarCameraData
  {
    glm::vec3 eye{0.0f};
    glm::vec3 center{0.0f, 0.0f, -1.0f};
    glm::vec3 up{0.0f, 1.0f, 0.0f};
    float     fov{60.0f};
  };

  void setup(const VkInstance& instance, const VkDevice& device, const VkPhysicalDevice& physicalDevice, uint32_t queueFamily) override;
  void createDescriptorSetLayout();
  void loadModel(ModelLoader& loader, glm::mat4 transform = glm::mat4(1));
  void updateDescriptorSet();
  void createUniformBuffer();
  void createObjDescriptionBuffer();
  void createTextureImages(const VkCommandBuffer&           cmdBuf,
                           const std::vector<std::string>& textures,
                           const std::vector<TextureAsset>& textureAssets);
  void updateUniformBuffer(const VkCommandBuffer& cmdBuf);
  void setMainCameraClipRange(float clipStart, float clipEnd);
  void setRadarCamera(const RadarCameraData& camera);
  void setHeightScanCamera(const RadarCameraData& camera);
  void setRadarLidarParams(const LidarParams& params);
  void setLidarEnabled(bool enabled);
  bool isLidarEnabled() const { return m_enableLidar; }
  void setHeightScanParams(const HeightScanParams& params);
  void setHeightScanEnabled(bool enabled);
  bool isHeightScanEnabled() const { return m_enableHeightScan; }
  void onResize(int /*w*/, int /*h*/);
  void destroyResources();
  std::optional<HeadlessAovTexture> GetAovTexture(HeadlessAov aov) const;

  struct ObjModel
  {
    uint32_t     nbIndices{0};
    uint32_t     nbVertices{0};
    nvvk::Buffer vertexBuffer;
    nvvk::Buffer indexBuffer;
    nvvk::Buffer matColorBuffer;
    nvvk::Buffer matIndexBuffer;
  };

  struct ObjInstance
  {
    glm::mat4 transform;
    uint32_t  objIndex{0};
  };

  uint32_t addInstance(const glm::mat4& transform, uint32_t objIndex, int instanceId = 0);
  size_t getInstanceCount() const { return m_instances.size(); }
  const ObjInstance& getInstance(size_t index) const { return m_instances[index]; }

  std::vector<ModelLoader> m_Loader;
  std::vector<ObjModel>    m_objModel;
  std::vector<ObjDesc>     m_objDesc;
  std::vector<ObjInstance> m_instances;

  nvvk::DescriptorSetBindings m_descSetLayoutBind;
  VkDescriptorPool            m_descPool;
  VkDescriptorSetLayout       m_descSetLayout;
  VkDescriptorSet             m_descSet;

  nvvk::Buffer m_bFrameUniforms;
  nvvk::Buffer m_bObjDesc;

  std::vector<nvvk::Texture> m_textures;

  nvvk::ResourceAllocatorDma m_alloc;
  nvvk::DebugUtil            m_debug;

  void     initRayTracing();
  auto     objectToVkGeometryKHR(const ObjModel& model);
  void     createBottomLevelAS();
  void     createTopLevelAS();
  void     createRtDescriptorSet();
  void     updateRtDescriptorSet();
  void     createRtPipeline();
  void     createLidarRtDescriptorSet();
  void     createLidarRtPipeline();
  void     createLidarRtShaderBindingTable();
  void     renderLidarPointCloud(const VkCommandBuffer& cmdBuf);
  void     createHeightScanRtPipeline();
  void     createHeightScanRtShaderBindingTable();
  void     renderHeightScanPointCloud(const VkCommandBuffer& cmdBuf);
  void     createLidarCompositePipeline();
  void     compositeLidar(const VkCommandBuffer& cmdBuf);
  void     raytrace(const VkCommandBuffer& cmdBuf);
  void     resetAccumulation();
  void     resetFrameHistory();
  uint32_t getAccumulatedFrames() const { return m_accumulatedFrames; }
  float    getMainCameraClipStart() const { return m_mainCameraClipStart; }
  float    getMainCameraClipEnd() const { return m_mainCameraClipEnd; }

  VkPhysicalDeviceRayTracingPipelinePropertiesKHR m_rtProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_PROPERTIES_KHR};
  nvvk::RaytracingBuilderKHR                        m_rtBuilder;
  nvvk::DescriptorSetBindings                       m_rtDescSetLayoutBind;
  VkDescriptorPool                                  m_rtDescPool{VK_NULL_HANDLE};
  VkDescriptorSetLayout                             m_rtDescSetLayout{VK_NULL_HANDLE};
  VkDescriptorSet                                   m_rtDescSet{VK_NULL_HANDLE};
  std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_rtShaderGroups;
  VkPipelineLayout                                  m_rtPipelineLayout{VK_NULL_HANDLE};
  VkPipeline                                        m_rtPipeline{VK_NULL_HANDLE};

  nvvk::DescriptorSetBindings                       m_lidarRtDescSetLayoutBind;
  VkDescriptorPool                                  m_lidarRtDescPool{VK_NULL_HANDLE};
  VkDescriptorSetLayout                             m_lidarRtDescSetLayout{VK_NULL_HANDLE};
  VkDescriptorSet                                   m_lidarRtDescSet{VK_NULL_HANDLE};
  std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_lidarRtShaderGroups;
  VkPipelineLayout                                  m_lidarRtPipelineLayout{VK_NULL_HANDLE};
  VkPipeline                                        m_lidarRtPipeline{VK_NULL_HANDLE};
  std::vector<VkRayTracingShaderGroupCreateInfoKHR> m_heightScanRtShaderGroups;
  VkPipelineLayout                                  m_heightScanRtPipelineLayout{VK_NULL_HANDLE};
  VkPipeline                                        m_heightScanRtPipeline{VK_NULL_HANDLE};

  nvvk::SBTWrapper                m_sbtWrapper;
  nvvk::Buffer                    m_rtSBTBuffer;
  VkStridedDeviceAddressRegionKHR m_rgenRegion{};
  VkStridedDeviceAddressRegionKHR m_missRegion{};
  VkStridedDeviceAddressRegionKHR m_hitRegion{};
  VkStridedDeviceAddressRegionKHR m_callRegion{};

  nvvk::Buffer                    m_lidarRtSBTBuffer;
  VkStridedDeviceAddressRegionKHR m_lidarRtRgenRegion{};
  VkStridedDeviceAddressRegionKHR m_lidarRtMissRegion{};
  VkStridedDeviceAddressRegionKHR m_lidarRtHitRegion{};
  VkStridedDeviceAddressRegionKHR m_lidarRtCallRegion{};

  nvvk::Buffer                    m_heightScanRtSBTBuffer;
  VkStridedDeviceAddressRegionKHR m_heightScanRtRgenRegion{};
  VkStridedDeviceAddressRegionKHR m_heightScanRtMissRegion{};
  VkStridedDeviceAddressRegionKHR m_heightScanRtHitRegion{};
  VkStridedDeviceAddressRegionKHR m_heightScanRtCallRegion{};

  std::vector<VkAccelerationStructureInstanceKHR>    m_tlas;
  std::vector<nvvk::RaytracingBuilderKHR::BlasInput> m_blas;

  // Push constant for ray tracer
  PushConstantRay m_pcRay{
      0,  // num lights
      0,  // frame index
      0.0f,  // jitter X
      0.0f,  // jitter Y
      6,  // max depth
      2,  // samples per frame
      eRaygenPassLowResBeauty,  // lidar pass mode
      {-90.0f, 90.0f, 0.5f, -2.0f, -20.0f, 1.0f, 2.0f, 200.0f},
      {-10.0f, 10.0f, 0.1f, -10.0f, 10.0f, 0.1f, 0.0f, 0.0f, -1.0f, 2.0f, 200.0f, 0.0f}};

  void animationInstances(float time);
  void animationObject(float time);

  void createCompDescriptors();
  void updateCompDescriptors(nvvk::Buffer& vertex);
  void createCompPipelines();

  nvvk::DescriptorSetBindings m_compDescSetLayoutBind;
  VkDescriptorPool            m_compDescPool{VK_NULL_HANDLE};
  VkDescriptorSetLayout       m_compDescSetLayout{VK_NULL_HANDLE};
  VkDescriptorSet             m_compDescSet{VK_NULL_HANDLE};
  VkPipeline                  m_compPipeline{VK_NULL_HANDLE};
  VkPipelineLayout            m_compPipelineLayout{VK_NULL_HANDLE};

  nvvk::DescriptorSetBindings m_lidarCompositeDescSetLayoutBind;
  VkDescriptorPool            m_lidarCompositeDescPool{VK_NULL_HANDLE};
  VkDescriptorSetLayout       m_lidarCompositeDescSetLayout{VK_NULL_HANDLE};
  VkDescriptorSet             m_lidarCompositeDescSet{VK_NULL_HANDLE};
  VkPipeline                  m_lidarCompositePipeline{VK_NULL_HANDLE};
  VkPipelineLayout            m_lidarCompositePipelineLayout{VK_NULL_HANDLE};

  VkBuildAccelerationStructureFlagsKHR m_rtFlags;

  void saveOffscreenColorToFile(const char* filename);

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

  void createOffscreenRender();

  nvvk::Texture          m_offscreenColor;
  VkFormat               m_offscreenColorFormat{VK_FORMAT_R32G32B32A32_SFLOAT};

  nvvk::Texture          m_offscreenObjectId;
  VkFormat               m_offscreenObjectIdFormat{VK_FORMAT_R32_SINT};

  nvvk::Texture          m_offscreenInstanceId;
  VkFormat               m_offscreenInstanceIdFormat{VK_FORMAT_R32_SINT};

  nvvk::Texture          m_offscreenDiffuseAlbedo;
  VkFormat               m_offscreenDiffuseAlbedoFormat{VK_FORMAT_R32G32B32A32_SFLOAT};

  nvvk::Texture          m_offscreenSpecularAlbedo;
  VkFormat               m_offscreenSpecularAlbedoFormat{VK_FORMAT_R32G32B32A32_SFLOAT};

  nvvk::Texture          m_offscreenNormalRoughness;
  VkFormat               m_offscreenNormalRoughnessFormat{VK_FORMAT_R32G32B32A32_SFLOAT};

  nvvk::Texture          m_offscreenMotionVector;
  VkFormat               m_offscreenMotionVectorFormat{VK_FORMAT_R32G32_SFLOAT};

  nvvk::Texture          m_offscreenLinearDepth;
  VkFormat               m_offscreenLinearDepthFormat{VK_FORMAT_R32_SFLOAT};

  nvvk::Texture          m_offscreenDepthAov;
  VkFormat               m_offscreenDepthAovFormat{VK_FORMAT_R32_SFLOAT};

  nvvk::Texture          m_offscreenSpecularHitDistance;
  VkFormat               m_offscreenSpecularHitDistanceFormat{VK_FORMAT_R32_SFLOAT};

  nvvk::Texture          m_offscreenLidarPointCloud;
  VkFormat               m_offscreenLidarPointCloudFormat{VK_FORMAT_R32G32B32A32_SFLOAT};
  nvvk::Texture          m_offscreenLidarPointCloudDepthKey;
  VkFormat               m_offscreenLidarPointCloudDepthKeyFormat{VK_FORMAT_R32_UINT};

  mutable nvvk::ExportResourceAllocatorDedicated m_sharedAlloc;
  VkExtent2D                                    m_renderSize{0, 0};
  VkExtent2D                                    m_aovSize{0, 0};
  float                                         m_mainCameraClipStart{0.1f};
  float                                         m_mainCameraClipEnd{1000.0f};

  nvvk::Texture m_offscreenDepth;
  VkFormat      m_offscreenDepthFormat{VK_FORMAT_X8_D24_UNORM_PACK32};

  std::vector<uint32_t> readObjectIdImage();

  void updateTlas(uint32_t mesh_Id, glm::mat4 transform, bool visible);
  void updateTlasEnd();
  void updateBlas(uint32_t mesh_Id);
  void updateMaterialAtRuntime(int modelIndex, int materialIndex, const WaveFrontMaterial& newMaterial);
  void updateMaterialsAtRuntime(const std::vector<MaterialUpdate>& updates);

  std::vector<Light> m_lights;
  std::vector<int>   m_instanceIds;
  std::vector<Light> m_uploadedLights;
  nvvk::Buffer       m_bInstanceIds;
  nvvk::Buffer       m_bLights;

  void addLight(const Light& light);
  void clearLights();
  void createLightBuffer();
  void updateLightBuffer(const VkCommandBuffer& cmdBuf);
  void createInstanceIdBuffer();
  void updateInstanceIdBuffer(const VkCommandBuffer& cmdBuf);

  std::vector<Sphere> m_spheres;
  nvvk::Buffer        m_spheresBuffer;
  nvvk::Buffer        m_spheresAabbBuffer;
  nvvk::Buffer        m_spheresMatColorBuffer;
  nvvk::Buffer        m_spheresMatIndexBuffer;

  void createSpheres(uint32_t nbSpheres);
  void addSpheres(std::vector<Sphere> vector);
  auto sphereToVkGeometryKHR();
  void createRtShaderBindingTable();

private:
  void createOffscreenImage(nvvk::Texture&    texture,
                            VkFormat          format,
                            VkImageUsageFlags usage,
                            VkExtent2D        extent = {0, 0});
  VkExtent2D computeRenderSize();
  void       refreshOffscreenRenderTargetsIfNeeded();
  void       refreshOffscreenRenderTargetDescriptors();
  void       updateLidarRtDescriptorSet();
  void       updateLidarCompositeDescriptorSet();

  uint32_t  m_accumulatedFrames{0};
  uint32_t  m_frameIndex{0};  // Monotonic jitter/RNG frame index; not reset by camera motion.
  glm::vec2 m_currentJitter{0.0f};
  glm::mat4 m_lastView{1.0f};
  glm::mat4 m_lastProj{1.0f};
  bool      m_hasLastCamera{false};
  RadarCameraData m_radarCamera{};
  bool            m_hasRadarCamera{false};
  RadarCameraData m_heightScanCamera{};
  bool            m_hasHeightScanCamera{false};
  bool            m_enableLidar{true};
  LidarParams     m_radarLidarParams{-90.0f, 90.0f, 0.5f, -2.0f, -20.0f, 1.0f, 2.0f, 200.0f};
  bool             m_enableHeightScan{true};
  HeightScanParams m_heightScanParams{-10.0f, 10.0f, 0.1f, -10.0f, 10.0f, 0.1f,
                                      0.0f, 0.0f, -1.0f, 2.0f, 200.0f, 0.0f};
};
