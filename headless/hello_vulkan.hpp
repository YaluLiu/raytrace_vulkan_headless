#pragma once

#include "aov_texture.hpp"

#include "headless_vk.hpp"
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/descriptorsets_vk.hpp"
#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include "shaders/host_device.h"

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
  void loadTextureAssets(const std::vector<TextureAsset>& textureAssets);
  void recreateTextureResources(const std::vector<TextureAsset>& textureAssets);
  void updateDescriptorSet();
  void createUniformBuffer();
  void createObjDescriptionBuffer();
  void createTextureImages(const VkCommandBuffer&           cmdBuf,
                           const std::vector<std::string>& textures,
                           const std::vector<TextureAsset>& textureAssets);
  void updateUniformBuffer(const VkCommandBuffer& cmdBuf);
  void setMainCameraClipRange(float clipStart, float clipEnd);
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
    bool      visible{true};
  };

  uint32_t addInstance(const glm::mat4& transform, uint32_t objIndex, int instanceId = 0);
  size_t getInstanceCount() const { return m_instances.size(); }
  const ObjInstance& getInstance(size_t index) const { return m_instances[index]; }

  std::vector<ModelLoader> m_Loader;
  std::vector<ObjModel>    m_objModel;
  std::vector<ObjDesc>     m_objDesc;
  std::vector<ObjInstance> m_instances;

  nvvk::DescriptorSetBindings m_descSetLayoutBind;
  VkDescriptorPool            m_descPool{VK_NULL_HANDLE};
  VkDescriptorSetLayout       m_descSetLayout{VK_NULL_HANDLE};
  VkDescriptorSet             m_descSet{VK_NULL_HANDLE};

  nvvk::Buffer m_bFrameUniforms;
  nvvk::Buffer m_bObjDesc;

  std::vector<nvvk::Texture> m_textures;

  nvvk::ResourceAllocatorDma m_alloc;
  nvvk::DebugUtil            m_debug;

  void     createRasterPipeline();
  void     rasterize(const VkCommandBuffer& cmdBuf);
  void     resetAccumulation();
  void     resetFrameHistory();
  uint32_t getAccumulatedFrames() const { return m_accumulatedFrames; }
  float    getMainCameraClipStart() const { return m_mainCameraClipStart; }
  float    getMainCameraClipEnd() const { return m_mainCameraClipEnd; }

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

  VkRenderPass     m_rasterRenderPass{VK_NULL_HANDLE};
  VkFramebuffer    m_rasterFramebuffer{VK_NULL_HANDLE};
  VkPipeline       m_rasterPipeline{VK_NULL_HANDLE};
  VkPipeline       m_domeBackgroundPipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_rasterPipelineLayout{VK_NULL_HANDLE};

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

    if(m_samplesPerFrame != spp)
    {
      m_samplesPerFrame = spp;
      resetAccumulation();
    }
  }
  int getSamplesPerFrame() const { return m_samplesPerFrame; }

  void createOffscreenRender();

  nvvk::Texture          m_offscreenColor;
  VkFormat               m_offscreenColorFormat{VK_FORMAT_R32G32B32A32_SFLOAT};

  nvvk::Texture          m_offscreenObjectId;
  VkFormat               m_offscreenObjectIdFormat{VK_FORMAT_R32_SINT};

  nvvk::Texture          m_offscreenInstanceId;
  VkFormat               m_offscreenInstanceIdFormat{VK_FORMAT_R32_SINT};

  nvvk::Texture          m_offscreenDepthAov;
  VkFormat               m_offscreenDepthAovFormat{VK_FORMAT_R32_SFLOAT};

  mutable nvvk::ExportResourceAllocatorDedicated m_sharedAlloc;
  VkExtent2D                                    m_renderSize{0, 0};
  VkExtent2D                                    m_aovSize{0, 0};
  float                                         m_mainCameraClipStart{0.1f};
  float                                         m_mainCameraClipEnd{1000.0f};

  nvvk::Texture m_offscreenDepth;
  VkFormat      m_offscreenDepthFormat{VK_FORMAT_X8_D24_UNORM_PACK32};

  std::vector<uint32_t> readObjectIdImage();

  void updateInstance(uint32_t instanceId, glm::mat4 transform, bool visible);
  void updateInstancesEnd();
  void updateMeshGeometry(uint32_t meshId);
  void updateMaterialAtRuntime(int modelIndex, int materialIndex, const WaveFrontMaterial& newMaterial);
  void updateMaterialsAtRuntime(const std::vector<MaterialUpdate>& updates);

  std::vector<Light> m_lights;
  std::vector<int>   m_instanceIds;
  std::vector<Light> m_uploadedLights;
  nvvk::Buffer       m_bLights;

  void addLight(const Light& light);
  void clearLights();
  void createLightBuffer();
  void updateLightBuffer(const VkCommandBuffer& cmdBuf);

private:
  void createOffscreenImage(nvvk::Texture&    texture,
                            VkFormat          format,
                            VkImageUsageFlags usage,
                            VkExtent2D        extent = {0, 0});
  VkExtent2D computeRenderSize();
  void       refreshOffscreenRenderTargetsIfNeeded();
  void       refreshOffscreenRenderTargetDescriptors();
  void       createRasterFramebuffer();
  void       destroyRasterFramebuffer();

  uint32_t  m_accumulatedFrames{0};
  uint32_t  m_frameIndex{0};  // Monotonic jitter/RNG frame index; not reset by camera motion.
  int       m_samplesPerFrame{2};
  glm::vec2 m_currentJitter{0.0f};
  glm::mat4 m_lastView{1.0f};
  glm::mat4 m_lastProj{1.0f};
  bool      m_hasLastCamera{false};
};
