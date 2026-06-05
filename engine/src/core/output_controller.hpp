#pragma once

#include <engine/aov_texture.hpp>
#include <engine/output_config.hpp>
#include "features/height_scan/height_scan_pass.hpp"
#include "features/lidar/lidar_point_cloud_pass.hpp"
#include "features/preview/preview_pipeline.hpp"
#include "features/tile/tile_atlas_pass.hpp"

#include <optional>
#include <string>

#include <vulkan/vulkan_core.h>

class GpuScene;
class SceneDescriptors;
class ViewUniforms;

class OutputController final
{
public:
  void setup(VkDevice device,
             VkPhysicalDevice physicalDevice,
             uint32_t graphicsQueueIndex,
             nvvk::ResourceAllocatorDma& allocator,
             nvvk::DebugUtil& debug);
  void destroy();

  void createPreviewTargets(VkExtent2D size);
  void resizePreview(VkExtent2D size);
  void rebuildPipelinesForSceneLayout(VkDescriptorSetLayout sceneDescriptorSetLayout);
  void destroyOutputPipelines();

  void recordTileAtlas(const VkCommandBuffer& cmdBuf,
                       const GpuScene& scene,
                       SceneDescriptors& descriptors,
                       ViewUniforms& viewUniforms);
  void recordPreviewAovs(const VkCommandBuffer& cmdBuf,
                         const GpuScene& scene,
                         const SceneDescriptors& descriptors);
  void recordLidarPointClouds(const VkCommandBuffer& cmdBuf, const GpuScene& scene);
  void recordHeightScans(const VkCommandBuffer& cmdBuf, const GpuScene& scene);
  void recordLidarPointOverlay(const VkCommandBuffer& cmdBuf, SceneDescriptors& descriptors);
  void recordHeightScanOverlay(const VkCommandBuffer& cmdBuf, SceneDescriptors& descriptors);
  std::optional<ExportedAovTexture> getAovTexture(Aov aov) const;

  void applyConfig(RendererOutputConfig config);
  LidarFramePointCloud readLidarPointCloudFrame();
  HeightScanFrame readHeightScanFrame();
  TileAtlasConfig getTileConfig() const { return m_tileAtlasPass.getConfig(); }
  void markTileAovAtlasConsumed(const std::string& outputDirectory = "output");
  bool isTileMultiviewSupported() const { return m_tileAtlasPass.isMultiviewSupported(); }
  uint32_t getTileMultiviewMaxViewCount() const { return m_tileAtlasPass.getMultiviewMaxViewCount(); }
  uint32_t getTileMultiviewEffectiveViewCount(uint32_t cameraCount, uint32_t capacity) const;

  PreviewPipeline& getPreviewPipeline() { return m_previewPipeline; }
  const PreviewPipeline& getPreviewPipeline() const { return m_previewPipeline; }

private:
  bool isTileAov(Aov aov) const;

  PreviewPipeline m_previewPipeline;
  TileAtlasPass m_tileAtlasPass;
  LidarPointCloudPass m_lidarPointCloudPass;
  HeightScanPass m_heightScanPass;
};
