#pragma once

#include "aov_texture.hpp"
#include "preview_raster_pipeline.hpp"
#include "tile_atlas_pass.hpp"
#include "tile_config.hpp"

#include <optional>
#include <string>

#include <vulkan/vulkan_core.h>

class RasterGpuScene;
class RasterSceneDescriptors;
class RasterViewUniforms;

class RasterOutputController final
{
public:
  void setup(VkDevice device,
             VkPhysicalDevice physicalDevice,
             uint32_t graphicsQueueIndex,
             nvvk::ResourceAllocatorDma& allocator,
             nvvk::DebugUtil& debug);
  void destroy();

  void createResources(VkExtent2D size);
  void resizePreview(VkExtent2D size);
  void createPreviewPipeline(VkDescriptorSetLayout sceneDescriptorSetLayout);
  void destroyPreviewPipeline();

  void recordTileAtlas(const VkCommandBuffer& cmdBuf,
                       const RasterGpuScene& scene,
                       RasterSceneDescriptors& descriptors,
                       RasterViewUniforms& viewUniforms);
  void recordPreviewAovs(const VkCommandBuffer& cmdBuf,
                         const RasterGpuScene& scene,
                         const RasterSceneDescriptors& descriptors);
  std::optional<ExportedRasterAovTexture> getAovTexture(RasterAov aov) const;

  void setTileConfig(TileAtlasConfig config);
  TileAtlasConfig getTileConfig() const { return m_tileAtlasPass.getConfig(); }
  void markTileAovAtlasConsumed(const std::string& outputDirectory = "output");
  bool isTileMultiviewSupported() const { return m_tileAtlasPass.isMultiviewSupported(); }
  uint32_t getTileMultiviewMaxViewCount() const { return m_tileAtlasPass.getMultiviewMaxViewCount(); }
  uint32_t getTileMultiviewEffectiveViewCount(uint32_t cameraCount, uint32_t capacity) const;

  PreviewRasterPipeline& getPreviewPipeline() { return m_previewRasterPipeline; }
  const PreviewRasterPipeline& getPreviewPipeline() const { return m_previewRasterPipeline; }

private:
  bool isTileAov(RasterAov aov) const;

  PreviewRasterPipeline m_previewRasterPipeline;
  TileAtlasPass m_tileAtlasPass;
};
