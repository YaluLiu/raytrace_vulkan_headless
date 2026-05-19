#pragma once

#include "aov_texture.hpp"

#include "headless_vk.hpp"
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/descriptorsets_vk.hpp"
#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include "preview_raster_pipeline.hpp"
#include "raster_scene_types.hpp"
#include "shaders/host_device.h"
#include "tile_aov_atlas.hpp"
#include "tile_config.hpp"
#include "multiview_tile_targets.hpp"
#include "multiview_tile_raster_pipeline.hpp"

#include "ModelLoader.h"

#include <optional>
#include <string>
#include <vector>

struct RasterCameraSpec
{
  std::string name;
  glm::vec3 position{0.0f, 0.0f, 0.0f};
  glm::vec3 forward{0.0f, 0.0f, -1.0f};
  glm::vec3 up{0.0f, 1.0f, 0.0f};
  float vfov_deg{45.0f};
  float clipStart{0.1f};
  float clipEnd{1000.0f};
};

struct RasterMaterialUpdate
{
  int modelIndex;
  int materialIndex;
  WaveFrontMaterial newMaterial;
};

class RasterRenderer : public nvvkhl::AppOffline
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
  void ensureFrameUniformCapacity(uint32_t slotCount);
  void setCameras(std::vector<RasterCameraSpec> cameras);
  const std::vector<RasterCameraSpec> &getCameras() const
  {
    return m_cameras;
  }
  void setMainCamera(const RasterCameraSpec &camera);
  void setMainCameraClipRange(float clipStart, float clipEnd);
  void setTileConfig(TileAtlasConfig config);
  TileAtlasConfig getTileConfig() const
  {
    return m_tileConfig;
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
  std::optional<ExportedRasterAovTexture> GetAovTexture(RasterAov aov) const;

  using ObjModel = RasterMeshBuffers;
  using ObjInstance = RasterInstance;

  uint32_t addInstance(const glm::mat4 &transform, uint32_t objIndex, int instanceId = 0);
  size_t getInstanceCount() const
  {
    return m_instances.size();
  }
  const ObjInstance &getInstance(size_t index) const
  {
    return m_instances[index];
  }
  ModelLoader &getMutableModelLoader(size_t meshId)
  {
    return m_Loader[meshId];
  }
  size_t getModelCount() const
  {
    return m_Loader.size();
  }

private:
  std::vector<ModelLoader> m_Loader;
  std::vector<ObjModel> m_objModel;
  std::vector<ObjDesc> m_objDesc;
  std::vector<ObjInstance> m_instances;

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

public:
  void createPreviewRasterPipeline();
  void renderPreviewAovs(const VkCommandBuffer &cmdBuf);
  void renderTileAovAtlas(const VkCommandBuffer &cmdBuf);
  void markTileAovAtlasConsumed(const std::string &outputDirectory = "output");
  float getMainCameraClipStart() const
  {
    return m_mainCameraClipStart;
  }
  float getMainCameraClipEnd() const
  {
    return m_mainCameraClipEnd;
  }

private:
  PreviewRasterPipeline m_previewRasterPipeline;
  TileAovAtlas m_tileAovAtlas;
  MultiviewTileTargets m_multiviewTileTargets;
  MultiviewTileRasterPipeline m_multiviewTileRasterPipeline;
  TileAtlasConfig m_tileConfig;
  bool m_tileMultiviewSupported{false};
  uint32_t m_tileMultiviewMaxViewCount{0};
  uint32_t m_tileMultiviewMaxInstanceIndex{0};
  uint32_t m_tileMultiviewEffectiveViewCount{0};
  bool m_tileMultiviewUnsupportedLogged{false};
  bool m_tileAtlasDirty{false};
  bool m_tileAtlasExportValid{false};

  std::vector<RasterCameraSpec> m_cameras;

public:
  void saveOffscreenColorToFile(const char *filename);

  void createOffscreenRender();
  uint32_t getRequiredFrameUniformSlots() const;
  void updateFrameUniformDescriptor();
  void ensureTileFrameUniformBuffer();
  void updateTileFrameUniformDescriptor();
  void updateTileFrameUniformBufferForBatch(const VkCommandBuffer &cmdBuf, uint32_t firstCameraIndex,
                                            uint32_t cameraCount, uint32_t viewCount, VkExtent2D tileExtent);
  uint32_t getTileMultiviewEffectiveViewCount(uint32_t cameraCount, uint32_t capacity) const;
  void logTileMultiviewUnavailableOnce(const char *reason);

private:
  float m_mainCameraClipStart{0.1f};
  float m_mainCameraClipEnd{1000.0f};

public:
  std::vector<uint32_t> readObjectIdImage();

  void updateInstance(uint32_t instanceId, glm::mat4 transform, bool visible);
  void updateMeshGeometry(uint32_t meshId);
  void updateMaterialsAtRuntime(const std::vector<RasterMaterialUpdate> &updates);

private:
  std::vector<Light> m_lights;
  std::vector<int> m_instanceIds;
  nvvk::Buffer m_bLights;

public:
  void addLight(const Light &light);
  void clearLights();
  void createLightBuffer();
  void updateLightBuffer(const VkCommandBuffer &cmdBuf);
};
