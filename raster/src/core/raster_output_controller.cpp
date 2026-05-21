#include "core/raster_output_controller.hpp"

#include "scene/raster_gpu_scene.hpp"
#include "scene/raster_scene_descriptors.hpp"
#include "scene/raster_view_uniforms.hpp"

void RasterOutputController::setup(VkDevice device,
                                   VkPhysicalDevice physicalDevice,
                                   uint32_t graphicsQueueIndex,
                                   nvvk::ResourceAllocatorDma& allocator,
                                   nvvk::DebugUtil& debug)
{
  m_previewRasterPipeline.setup(device, physicalDevice, graphicsQueueIndex, allocator, debug);
  m_tileAtlasPass.setup(device, physicalDevice, graphicsQueueIndex, allocator, debug);
}

void RasterOutputController::destroy()
{
  m_tileAtlasPass.destroy();
  m_previewRasterPipeline.destroy();
}

void RasterOutputController::createResources(VkExtent2D size)
{
  m_previewRasterPipeline.createOffscreenRender(size);
}

void RasterOutputController::resizePreview(VkExtent2D size)
{
  m_previewRasterPipeline.createOffscreenRender(size);
}

void RasterOutputController::createPreviewPipeline(VkDescriptorSetLayout sceneDescriptorSetLayout)
{
  m_previewRasterPipeline.createGraphicsPipeline(sceneDescriptorSetLayout);
  m_tileAtlasPass.destroyGraphicsPipeline();
}

void RasterOutputController::destroyPreviewPipeline()
{
  m_previewRasterPipeline.destroyGraphicsPipeline();
  m_tileAtlasPass.destroyGraphicsPipeline();
}

void RasterOutputController::recordTileAtlas(const VkCommandBuffer& cmdBuf,
                                             const RasterGpuScene& scene,
                                             RasterSceneDescriptors& descriptors,
                                             RasterViewUniforms& viewUniforms)
{
  m_tileAtlasPass.record(cmdBuf, scene, descriptors, viewUniforms, m_previewRasterPipeline);
}

void RasterOutputController::recordPreviewAovs(const VkCommandBuffer& cmdBuf,
                                               const RasterGpuScene& scene,
                                               const RasterSceneDescriptors& descriptors)
{
  m_previewRasterPipeline.renderPreviewAovs(cmdBuf, descriptors.set(), scene.getMeshBuffers(), scene.getInstances(),
                                            scene.getInstanceIds());
}

std::optional<ExportedRasterAovTexture> RasterOutputController::getAovTexture(RasterAov aov) const
{
  if(isTileAov(aov))
  {
    return m_tileAtlasPass.getAovTexture(aov);
  }
  return m_previewRasterPipeline.getAovTexture(aov);
}

void RasterOutputController::setTileConfig(TileAtlasConfig config)
{
  m_tileAtlasPass.setConfig(config);
}

void RasterOutputController::setRequestedTileAovChannels(TileAovChannelMask channels)
{
  m_tileAtlasPass.setRequestedChannels(channels);
}

void RasterOutputController::markTileAovAtlasConsumed(const std::string& outputDirectory)
{
  m_tileAtlasPass.markConsumed(outputDirectory);
}

uint32_t RasterOutputController::getTileMultiviewEffectiveViewCount(uint32_t cameraCount, uint32_t capacity) const
{
  return m_tileAtlasPass.getEffectiveViewCount(cameraCount, capacity);
}

bool RasterOutputController::isTileAov(RasterAov aov) const
{
  return aov == RasterAov::TileColor || aov == RasterAov::TileDepth || aov == RasterAov::TileDisplayColor ||
         aov == RasterAov::TileDisplayDepth;
}
