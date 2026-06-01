#pragma once

#include <raster/aov_texture.hpp>
#include <raster/height_scan_types.hpp>
#include <raster/lidar_types.hpp>
#include <raster/mesh_types.hpp>
#include <raster/raster_renderer_types.hpp>
#include <raster/texture_asset.hpp>
#include <raster/tile_config.hpp>

#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <vulkan/vulkan_core.h>

namespace raster
{
class RasterRendererAccess;
}

class RasterRenderer
{
public:
  RasterRenderer();
  ~RasterRenderer();
  RasterRenderer(const RasterRenderer&) = delete;
  RasterRenderer& operator=(const RasterRenderer&) = delete;
  RasterRenderer(RasterRenderer&&) noexcept;
  RasterRenderer& operator=(RasterRenderer&&) noexcept;

  // Lifecycle.
  void setup(const VkInstance &instance, const VkDevice &device, const VkPhysicalDevice &physicalDevice,
             uint32_t queueFamily);
  void create(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, uint32_t queueFamily, VkExtent2D size);
  void destroy();
  VkDevice getDevice() const;
  const std::vector<VkCommandBuffer>& getCommandBuffers() const;
  uint32_t getCurFrame() const;
  void submitCurrentCommandBufferAndWait();
  void onResize(int /*w*/, int /*h*/);

  // Scene upload/update facade.
  void uploadMesh(const RasterMeshGeometry &geometry,
                  std::span<const RasterMaterial> materials,
                  glm::mat4 transform = glm::mat4(1));
  void loadTextureAssets(const std::vector<TextureAsset> &textureAssets);
  void rebuildTextureResourcesAndSceneBindings(const std::vector<TextureAsset> &textureAssets);
  uint32_t addInstance(const glm::mat4 &transform, uint32_t objIndex, int instanceId = 0);
  void updateInstance(uint32_t instanceId,
                      glm::mat4 transform,
                      bool visible,
                      uint32_t traceMask = kRasterTraceMaskDefaultGeometry);
  void updateMeshGeometry(uint32_t meshId, const RasterMeshGeometry &geometry);
  void updateMaterialsAtRuntime(const std::vector<RasterMaterialUpdate> &updates);
  void createRayTracingResources();
  void destroyRayTracingResources();
  void flushRayTracingUpdates();
  bool hasRayTracingTlas() const;
  VkAccelerationStructureKHR getRayTracingTlas() const;
  std::optional<RasterTlasDescriptorInfo> getRayTracingTlasDescriptorInfo() const;
  void addLight(const Light &light);
  void clearLights();

  // View configuration.
  void setCameras(std::vector<RasterCameraSpec> cameras);
  const std::vector<RasterCameraSpec> &getCameras() const;
  void setMainCamera(const RasterCameraSpec &camera);
  void setMainCameraClipRange(float clipStart, float clipEnd);
  float getMainCameraClipStart() const;
  float getMainCameraClipEnd() const;

  // Output configuration/query.
  void setTileConfig(TileAtlasConfig config);
  void setRequestedTileAovChannels(TileAovChannelMask channels);
  TileAtlasConfig getTileConfig() const;
  bool isTileMultiviewSupported() const;
  uint32_t getTileMultiviewMaxViewCount() const;
  std::optional<ExportedRasterAovTexture> GetAovTexture(RasterAov aov) const;
  void setLidarSensors(std::vector<RasterLidarSensorSpec> sensors);
  void setLidarVisualizationConfig(RasterLidarVisualizationConfig config);
  RasterLidarFramePointCloud readLidarPointCloudFrame();
  void setHeightScanSensors(std::vector<RasterHeightScanSensorSpec> sensors);
  void setHeightScanVisualizationConfig(RasterHeightScanVisualizationConfig config);
  RasterHeightScanFrame readHeightScanFrame();

  // Debug helpers.
  std::vector<uint32_t> readObjectIdImage();
  void saveOffscreenColorToFile(const char *filename);

  size_t getInstanceCount() const;
  RasterInstanceInfo getInstance(size_t index) const;
  size_t getMeshSourceCount() const;

private:
  friend class raster::RasterRendererAccess;

  struct Impl;
  std::unique_ptr<Impl> m_impl;
};
