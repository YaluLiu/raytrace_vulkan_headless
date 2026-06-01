#pragma once

#include <engine/aov_texture.hpp>
#include <engine/output_config.hpp>
#include "features/tile/multiview_tile_pipeline.hpp"
#include "features/tile/multiview_tile_targets.hpp"
#include "features/tile/tile_aov_channel.hpp"

#include <optional>
#include <string>

#include <vulkan/vulkan_core.h>

class PreviewPipeline;
class GpuScene;
class SceneDescriptors;
class ViewUniforms;

class TileAtlasPass final
{
public:
  void setup(VkDevice device,
             VkPhysicalDevice physicalDevice,
             uint32_t graphicsQueueIndex,
             nvvk::ResourceAllocatorDma& allocator,
             nvvk::DebugUtil& debug);
  void destroy();
  void destroyGraphicsPipeline();

  void configure(TileOutputConfig config);
  TileAtlasConfig getConfig() const { return m_config; }
  void record(const VkCommandBuffer& cmdBuf,
              const GpuScene& scene,
              SceneDescriptors& descriptors,
              ViewUniforms& viewUniforms,
              const PreviewPipeline& previewPipeline);
  std::optional<ExportedAovTexture> getAovTexture(Aov aov) const;
  void markConsumed(const std::string& outputDirectory = "output");

  bool isMultiviewSupported() const { return m_multiviewSupported; }
  uint32_t getMultiviewMaxViewCount() const { return m_multiviewMaxViewCount; }
  uint32_t getEffectiveViewCount(uint32_t cameraCount, uint32_t capacity) const;

private:
  void logUnavailableOnce(const char* reason);
  TileAovChannelMask effectiveChannels() const;

  TileAovChannelAtlas m_colorAtlas;
  TileAovChannelAtlas m_depthAtlas;
  MultiviewTileTargets m_multiviewTileTargets;
  MultiviewTilePipeline m_multiviewTilePipeline;
  TileAtlasConfig m_config;
  TileAovChannelMask m_requestedChannels{TileAovChannelMask::ColorDepth()};
  bool m_multiviewSupported{false};
  uint32_t m_multiviewMaxViewCount{0};
  uint32_t m_multiviewMaxInstanceIndex{0};
  uint32_t m_multiviewEffectiveViewCount{0};
  bool m_multiviewUnsupportedLogged{false};
  bool m_colorDirty{false};
  bool m_depthDirty{false};
  bool m_colorExportValid{false};
  bool m_depthExportValid{false};
};
