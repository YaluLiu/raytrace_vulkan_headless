#include "core/output_controller.hpp"

#include "scene/gpu_scene.hpp"
#include "scene/scene_descriptors.hpp"
#include "scene/view_uniforms.hpp"

#include <utility>

void OutputController::setup(VkDevice device,
                                   VkPhysicalDevice physicalDevice,
                                   uint32_t graphicsQueueIndex,
                                   nvvk::ResourceAllocatorDma& allocator,
                                   nvvk::DebugUtil& debug)
{
  m_previewPipeline.setup(device, physicalDevice, graphicsQueueIndex, allocator, debug);
  m_tileAtlasPass.setup(device, physicalDevice, graphicsQueueIndex, allocator, debug);
  m_lidarPointCloudPass.setup(device, physicalDevice, graphicsQueueIndex, allocator, debug);
  m_heightScanPass.setup(device, physicalDevice, graphicsQueueIndex, allocator, debug);
}

void OutputController::destroy()
{
  m_heightScanPass.destroy();
  m_lidarPointCloudPass.destroy();
  m_tileAtlasPass.destroy();
  m_previewPipeline.destroy();
}

void OutputController::createPreviewTargets(VkExtent2D size)
{
  m_previewPipeline.recreateAovTargets(size);
}

void OutputController::resizePreview(VkExtent2D size)
{
  m_previewPipeline.recreateAovTargets(size);
}

void OutputController::rebuildPipelinesForSceneLayout(VkDescriptorSetLayout sceneDescriptorSetLayout)
{
  m_previewPipeline.createGraphicsPipeline(sceneDescriptorSetLayout);
  m_tileAtlasPass.destroyGraphicsPipeline();
  m_lidarPointCloudPass.rebuildPipelinesForSceneLayout(sceneDescriptorSetLayout, m_previewPipeline);
  m_heightScanPass.rebuildPipelinesForSceneLayout(sceneDescriptorSetLayout, m_previewPipeline);
}

void OutputController::destroyOutputPipelines()
{
  m_previewPipeline.destroyGraphicsPipeline();
  m_tileAtlasPass.destroyGraphicsPipeline();
  m_lidarPointCloudPass.destroyGraphicsPipeline();
  m_heightScanPass.destroyGraphicsPipeline();
}

void OutputController::recordTileAtlas(const VkCommandBuffer& cmdBuf,
                                             const GpuScene& scene,
                                             SceneDescriptors& descriptors,
                                             ViewUniforms& viewUniforms)
{
  m_tileAtlasPass.record(cmdBuf, scene, descriptors, viewUniforms, m_previewPipeline);
}

void OutputController::recordPreviewAovs(const VkCommandBuffer& cmdBuf,
                                               const GpuScene& scene,
                                               const SceneDescriptors& descriptors)
{
  m_previewPipeline.recordPreviewAovs(cmdBuf, descriptors.set(), scene.getMeshBuffers(), scene.getInstances(),
                                            scene.getInstanceIds());
}

void OutputController::recordLidarPointClouds(const VkCommandBuffer& cmdBuf,
                                                    const GpuScene& scene,
                                                    SceneDescriptors& descriptors,
                                                    const PreviewPipeline& previewPipeline)
{
  (void)descriptors;
  (void)previewPipeline;
  m_lidarPointCloudPass.recordGenerate(cmdBuf, scene.getRayTracingTlasDescriptorInfo());
}

void OutputController::recordHeightScans(const VkCommandBuffer& cmdBuf, const GpuScene& scene)
{
  m_heightScanPass.recordGenerate(cmdBuf, scene.getRayTracingTlasDescriptorInfo());
}

void OutputController::recordLidarPointOverlay(const VkCommandBuffer& cmdBuf, SceneDescriptors& descriptors)
{
  m_lidarPointCloudPass.recordOverlay(cmdBuf, descriptors.set(), descriptors.layout(), m_previewPipeline);
}

void OutputController::recordHeightScanOverlay(const VkCommandBuffer& cmdBuf, SceneDescriptors& descriptors)
{
  m_heightScanPass.recordOverlay(cmdBuf, descriptors.set(), descriptors.layout(), m_previewPipeline);
}

std::optional<ExportedAovTexture> OutputController::getAovTexture(Aov aov) const
{
  if(isTileAov(aov))
  {
    return m_tileAtlasPass.getAovTexture(aov);
  }
  return m_previewPipeline.getAovTexture(aov);
}

void OutputController::setTileConfig(TileAtlasConfig config)
{
  m_tileAtlasPass.setConfig(config);
}

void OutputController::setLidarSensors(std::vector<LidarSensorSpec> sensors)
{
  m_lidarPointCloudPass.setSensors(std::move(sensors));
}

void OutputController::setLidarVisualizationConfig(LidarVisualizationConfig config)
{
  m_lidarPointCloudPass.setVisualizationConfig(config);
}

LidarFramePointCloud OutputController::readLidarPointCloudFrame()
{
  return m_lidarPointCloudPass.readPointCloudFrame();
}

void OutputController::setHeightScanSensors(std::vector<HeightScanSensorSpec> sensors)
{
  m_heightScanPass.setSensors(std::move(sensors));
}

void OutputController::setHeightScanVisualizationConfig(HeightScanVisualizationConfig config)
{
  m_heightScanPass.setVisualizationConfig(config);
}

HeightScanFrame OutputController::readHeightScanFrame()
{
  return m_heightScanPass.readHeightScanFrame();
}

void OutputController::setRequestedTileAovChannels(TileAovChannelMask channels)
{
  m_tileAtlasPass.setRequestedChannels(channels);
}

void OutputController::markTileAovAtlasConsumed(const std::string& outputDirectory)
{
  m_tileAtlasPass.markConsumed(outputDirectory);
}

uint32_t OutputController::getTileMultiviewEffectiveViewCount(uint32_t cameraCount, uint32_t capacity) const
{
  return m_tileAtlasPass.getEffectiveViewCount(cameraCount, capacity);
}

bool OutputController::isTileAov(Aov aov) const
{
  return aov == Aov::TileColor || aov == Aov::TileDepth;
}
