#pragma once

#include "aov_texture.hpp"

#include "headless_vk.hpp"
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/descriptorsets_vk.hpp"
#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include "raster_pipeline.hpp"
#include "raster_scene_types.hpp"
#include "shaders/host_device.h"
#include "tile_atlas.hpp"
#include "tile_config.hpp"
#include "tile_layer_output.hpp"
#include "tile_multiview_raster_pipeline.hpp"

#include "ModelLoader.h"

#include <optional>
#include <string>
#include <vector>

struct HeadlessCameraData
{
  std::string name;
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  glm::vec3 forward{0.0f, 0.0f, -1.0f};
  glm::vec3 up{0.0f, 1.0f, 0.0f};
  float vfov_deg{45.0f};
  float clipStart{0.1f};
  float clipEnd{1000.0f};
};

struct MaterialUpdate
{
  int modelIndex;
  int materialIndex;
  WaveFrontMaterial newMaterial;
};

class HelloVulkan : public nvvkhl::AppOffline
{
public:
  void setup(const VkInstance &instance, const VkDevice &device, const VkPhysicalDevice &physicalDevice,
             uint32_t queueFamily) override;
  void createDescriptorSetLayout();
  void loadModel(ModelLoader &loader, glm::mat4 transform = glm::mat4(1));
  void loadTextureAssets(const std::vector<TextureAsset> &textureAssets);
  void recreateTextureResources(const std::vector<TextureAsset> &textureAssets);
  void updateDescriptorSet();
  void createUniformBuffer();
  void createObjDescriptionBuffer();
  void createTextureImages(const VkCommandBuffer &cmdBuf, const std::vector<std::string> &textures,
                           const std::vector<TextureAsset> &textureAssets);
  void updateUniformBuffer(const VkCommandBuffer &cmdBuf);
  void updateUniformBufferForExtent(const VkCommandBuffer &cmdBuf, VkExtent2D renderSize);
  void updateUniformBufferForCamera(const VkCommandBuffer &cmdBuf, const HeadlessCameraData &camera,
                                    VkExtent2D renderSize, uint32_t slotIndex = 0);
  void ensureFrameUniformCapacity(uint32_t slotCount);
  void setCameras(std::vector<HeadlessCameraData> cameras);
  const std::vector<HeadlessCameraData> &getCameras() const
  {
    return m_cameras;
  }
  void setMainCamera(const HeadlessCameraData &camera);
  void setMainCameraClipRange(float clipStart, float clipEnd);
  void setTileConfig(HeadlessTileConfig config);
  HeadlessTileConfig getTileConfig() const
  {
    return m_tileConfig;
  }
  void setTileRenderMode(HeadlessTileRenderMode mode)
  {
    m_tileRenderMode = mode;
  }
  HeadlessTileRenderMode getTileRenderMode() const
  {
    return m_tileRenderMode;
  }
  bool isTileMultiviewSupported() const
  {
    return m_tileMultiviewSupported;
  }
  uint32_t getTileMultiviewMaxViewCount() const
  {
    return m_tileMultiviewMaxViewCount;
  }
  void onResize(int /*w*/, int /*h*/);
  void destroyResources();
  std::optional<HeadlessAovTexture> GetAovTexture(HeadlessAov aov) const;

  using ObjModel = RasterObjModel;
  using ObjInstance = RasterObjInstance;

  uint32_t addInstance(const glm::mat4 &transform, uint32_t objIndex, int instanceId = 0);
  size_t getInstanceCount() const
  {
    return m_instances.size();
  }
  const ObjInstance &getInstance(size_t index) const
  {
    return m_instances[index];
  }

  std::vector<ModelLoader> m_Loader;
  std::vector<ObjModel> m_objModel;
  std::vector<ObjDesc> m_objDesc;
  std::vector<ObjInstance> m_instances;
  std::vector<glm::mat4> m_animationBaseTransforms;

  nvvk::DescriptorSetBindings m_descSetLayoutBind;
  VkDescriptorPool m_descPool{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_descSetLayout{VK_NULL_HANDLE};
  VkDescriptorSet m_descSet{VK_NULL_HANDLE};

  nvvk::Buffer m_bFrameUniforms;
  VkDeviceSize m_frameUniformStride{sizeof(FrameUniforms)};
  uint32_t m_frameUniformSlotCount{0};
  nvvk::Buffer m_bTileFrameUniforms;
  nvvk::Buffer m_bObjDesc;

  std::vector<nvvk::Texture> m_textures;

  nvvk::ResourceAllocatorDma m_alloc;
  nvvk::DebugUtil m_debug;

  void createRasterPipeline();
  void rasterize(const VkCommandBuffer &cmdBuf);
  void renderTileAtlas(const VkCommandBuffer &cmdBuf);
  void renderTileAtlasMultiview(const VkCommandBuffer &cmdBuf);
  void saveTileAtlasOutputs(const std::string &outputDirectory = "output");
  float getMainCameraClipStart() const
  {
    return m_mainCameraClipStart;
  }
  float getMainCameraClipEnd() const
  {
    return m_mainCameraClipEnd;
  }

  void animationInstances(float time);
  void animationObject(float time);

  void createCompDescriptors();
  void updateCompDescriptors(nvvk::Buffer &vertex);
  void createCompPipelines();

  nvvk::DescriptorSetBindings m_compDescSetLayoutBind;
  VkDescriptorPool m_compDescPool{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_compDescSetLayout{VK_NULL_HANDLE};
  VkDescriptorSet m_compDescSet{VK_NULL_HANDLE};
  VkPipeline m_compPipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_compPipelineLayout{VK_NULL_HANDLE};

  RasterPipeline m_mainRasterPipeline;
  RasterPipeline m_tileRasterPipeline;
  TileAtlasOutput m_tileAtlas;
  TileLayerOutput m_tileLayerOutput;
  TileMultiviewRasterPipeline m_tileMultiviewRasterPipeline;
  HeadlessTileConfig m_tileConfig;
  HeadlessTileRenderMode m_tileRenderMode{HeadlessTileRenderMode::MultiviewPreferred};
  bool m_tileMultiviewSupported{false};
  uint32_t m_tileMultiviewMaxViewCount{0};
  uint32_t m_tileMultiviewMaxInstanceIndex{0};
  uint32_t m_tileMultiviewEffectiveViewCount{0};
  bool m_tileMultiviewUnsupportedLogged{false};
  bool m_tileAtlasDirty{false};
  bool m_tileAtlasExportValid{false};

  std::vector<HeadlessCameraData> m_cameras;

  void saveOffscreenColorToFile(const char *filename);

  void createOffscreenRender();
  uint32_t getRequiredFrameUniformSlots() const;
  uint32_t getFrameUniformOffset(uint32_t slotIndex) const;
  void updateFrameUniformDescriptor();
  void ensureTileFrameUniformBuffer();
  void updateTileFrameUniformDescriptor();
  void updateTileFrameUniformBufferForBatch(const VkCommandBuffer &cmdBuf, uint32_t firstCameraIndex,
                                            uint32_t cameraCount, uint32_t viewCount, VkExtent2D tileExtent);
  uint32_t getTileMultiviewEffectiveViewCount(uint32_t cameraCount, uint32_t capacity) const;
  void logTileMultiviewUnavailableOnce(const char *reason);

  float m_mainCameraClipStart{0.1f};
  float m_mainCameraClipEnd{1000.0f};

  std::vector<uint32_t> readObjectIdImage();

  void updateInstance(uint32_t instanceId, glm::mat4 transform, bool visible);
  void updateMeshGeometry(uint32_t meshId);
  void updateMaterialsAtRuntime(const std::vector<MaterialUpdate> &updates);

  std::vector<Light> m_lights;
  std::vector<int> m_instanceIds;
  nvvk::Buffer m_bLights;

  void addLight(const Light &light);
  void clearLights();
  void createLightBuffer();
  void updateLightBuffer(const VkCommandBuffer &cmdBuf);
};
