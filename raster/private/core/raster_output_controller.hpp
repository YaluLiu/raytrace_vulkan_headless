#pragma once

#include <raster/aov_texture.hpp>
#include <raster/height_scan_types.hpp>
#include <raster/lidar_types.hpp>
#include "output/height_scan/height_scan_pass.hpp"
#include "output/lidar/lidar_point_cloud_pass.hpp"
#include "output/preview_raster_pipeline.hpp"
#include "output/tile/tile_atlas_pass.hpp"
#include <raster/tile_config.hpp>

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

  void createPreviewTargets(VkExtent2D size);
  void resizePreview(VkExtent2D size);
  void rebuildPipelinesForSceneLayout(VkDescriptorSetLayout sceneDescriptorSetLayout);
  void destroyOutputPipelines();

  void recordTileAtlas(const VkCommandBuffer& cmdBuf,
                       const RasterGpuScene& scene,
                       RasterSceneDescriptors& descriptors,
                       RasterViewUniforms& viewUniforms);
  void recordPreviewAovs(const VkCommandBuffer& cmdBuf,
                         const RasterGpuScene& scene,
                         const RasterSceneDescriptors& descriptors);
  void recordLidarPointClouds(const VkCommandBuffer& cmdBuf,
                              const RasterGpuScene& scene,
                              RasterSceneDescriptors& descriptors,
                              const PreviewRasterPipeline& previewPipeline);
  void recordHeightScans(const VkCommandBuffer& cmdBuf, const RasterGpuScene& scene);
  void recordLidarPointOverlay(const VkCommandBuffer& cmdBuf, RasterSceneDescriptors& descriptors);
  void recordHeightScanOverlay(const VkCommandBuffer& cmdBuf, RasterSceneDescriptors& descriptors);
  std::optional<ExportedRasterAovTexture> getAovTexture(RasterAov aov) const;

  void setLidarSensors(std::vector<RasterLidarSensorSpec> sensors);
  void setLidarVisualizationConfig(RasterLidarVisualizationConfig config);
  RasterLidarFramePointCloud readLidarPointCloudFrame();
  void setHeightScanSensors(std::vector<RasterHeightScanSensorSpec> sensors);
  void setHeightScanVisualizationConfig(RasterHeightScanVisualizationConfig config);
  RasterHeightScanFrame readHeightScanFrame();
  void setTileConfig(TileAtlasConfig config);
  void setRequestedTileAovChannels(TileAovChannelMask channels);
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
  LidarPointCloudPass m_lidarPointCloudPass;
  HeightScanPass m_heightScanPass;
};
