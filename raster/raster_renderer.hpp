#pragma once

#include "aov_texture.hpp"

#include "headless_vk.hpp"
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include "raster_gpu_scene.hpp"
#include "raster_output_controller.hpp"
#include "raster_renderer_types.hpp"
#include "raster_scene_descriptors.hpp"
#include "raster_view_uniforms.hpp"
#include "tile_config.hpp"

#include "ModelLoader.h"

#include <optional>
#include <string>
#include <vector>

class RasterRenderer : public nvvkhl::AppOffline
{
public:
  // Lifecycle.
  void setup(const VkInstance &instance, const VkDevice &device, const VkPhysicalDevice &physicalDevice,
             uint32_t queueFamily) override;
  void onResize(int /*w*/, int /*h*/);
  void destroyResources();

  // Scene upload/update facade.
  void loadModel(ModelLoader &loader, glm::mat4 transform = glm::mat4(1));
  void loadTextureAssets(const std::vector<TextureAsset> &textureAssets);
  void recreateTextureResources(const std::vector<TextureAsset> &textureAssets);
  void createTextureImages(const VkCommandBuffer &cmdBuf, const std::vector<std::string> &textures,
                           const std::vector<TextureAsset> &textureAssets);
  uint32_t addInstance(const glm::mat4 &transform, uint32_t objIndex, int instanceId = 0);
  void updateInstance(uint32_t instanceId, glm::mat4 transform, bool visible);
  void updateMeshGeometry(uint32_t meshId);
  void updateMaterialsAtRuntime(const std::vector<RasterMaterialUpdate> &updates);
  void addLight(const Light &light);
  void clearLights();

  // View configuration.
  void setCameras(std::vector<RasterCameraSpec> cameras);
  const std::vector<RasterCameraSpec> &getCameras() const
  {
    return m_viewUniforms.getCameras();
  }
  void setMainCamera(const RasterCameraSpec &camera);
  void setMainCameraClipRange(float clipStart, float clipEnd);
  float getMainCameraClipStart() const
  {
    return m_viewUniforms.getMainCameraClipStart();
  }
  float getMainCameraClipEnd() const
  {
    return m_viewUniforms.getMainCameraClipEnd();
  }

  // Output configuration/query.
  void setTileConfig(TileAtlasConfig config);
  void setRequestedTileAovChannels(TileAovChannelMask channels);
  TileAtlasConfig getTileConfig() const
  {
    return m_outputController.getTileConfig();
  }
  bool isTileMultiviewSupported() const
  {
    return m_outputController.isTileMultiviewSupported();
  }
  uint32_t getTileMultiviewMaxViewCount() const
  {
    return m_outputController.getTileMultiviewMaxViewCount();
  }
  std::optional<ExportedRasterAovTexture> GetAovTexture(RasterAov aov) const;

  // Debug helpers.
  std::vector<uint32_t> readObjectIdImage();
  void saveOffscreenColorToFile(const char *filename);

  using ObjModel = RasterMeshBuffers;
  using ObjInstance = RasterInstance;

  size_t getInstanceCount() const
  {
    return m_gpuScene.getInstanceCount();
  }
  const ObjInstance &getInstance(size_t index) const
  {
    return m_gpuScene.getInstance(index);
  }
  ModelLoader &getMutableModelLoader(size_t meshId)
  {
    return m_gpuScene.getMutableModelLoader(meshId);
  }
  size_t getModelCount() const
  {
    return m_gpuScene.getModelCount();
  }

  // Temporary internal API used by the session/frame executor while renderer
  // internals are split into dedicated components.
  void createDescriptorSetLayout();
  void updateDescriptorSet();
  void createUniformBuffer();
  void createObjDescriptionBuffer();
  void createPreviewRasterPipeline();
  void renderPreviewAovs(const VkCommandBuffer &cmdBuf);
  void renderTileAovAtlas(const VkCommandBuffer &cmdBuf);
  void markTileAovAtlasConsumed(const std::string &outputDirectory = "output");
  void createOffscreenRender();
  void updateUniformBuffer(const VkCommandBuffer &cmdBuf);
  void updateUniformBufferForExtent(const VkCommandBuffer &cmdBuf, VkExtent2D renderSize);
  void ensureFrameUniformCapacity(uint32_t slotCount);
  uint32_t getRequiredFrameUniformSlots() const;
  void updateFrameUniformDescriptor();
  void ensureTileFrameUniformBuffer();
  void updateTileFrameUniformDescriptor();
  void updateTileFrameUniformBufferForBatch(const VkCommandBuffer &cmdBuf, uint32_t firstCameraIndex,
                                            uint32_t cameraCount, uint32_t viewCount, VkExtent2D tileExtent);
  uint32_t getTileMultiviewEffectiveViewCount(uint32_t cameraCount, uint32_t capacity) const;
  void logTileMultiviewUnavailableOnce(const char *reason);
  void createLightBuffer();
  void updateLightBuffer(const VkCommandBuffer &cmdBuf);

private:
  nvvk::ResourceAllocatorDma m_alloc;
  nvvk::DebugUtil m_debug;
  RasterGpuScene m_gpuScene;
  RasterSceneDescriptors m_sceneDescriptors;
  RasterViewUniforms m_viewUniforms;
  RasterOutputController m_outputController;
};
