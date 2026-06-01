#pragma once

#include <engine/aov_texture.hpp>
#include <engine/height_scan_types.hpp>
#include <engine/lidar_types.hpp>
#include <engine/mesh_types.hpp>
#include <engine/renderer_types.hpp>
#include <engine/texture_asset.hpp>
#include <engine/tile_config.hpp>

#include <memory>
#include <optional>
#include <span>
#include <vector>

#include <vulkan/vulkan_core.h>

namespace engine
{
class RendererAccess;
}

class Renderer
{
public:
  Renderer();
  ~Renderer();
  Renderer(const Renderer&) = delete;
  Renderer& operator=(const Renderer&) = delete;
  Renderer(Renderer&&) noexcept;
  Renderer& operator=(Renderer&&) noexcept;

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
  void uploadMesh(const MeshGeometry &geometry,
                  std::span<const Material> materials,
                  glm::mat4 transform = glm::mat4(1));
  void loadTextureAssets(const std::vector<TextureAsset> &textureAssets);
  void rebuildTextureResourcesAndSceneBindings(const std::vector<TextureAsset> &textureAssets);
  uint32_t addInstance(const glm::mat4 &transform, uint32_t objIndex, int instanceId = 0);
  void updateInstance(uint32_t instanceId,
                      glm::mat4 transform,
                      bool visible,
                      uint32_t traceMask = kTraceMaskDefaultGeometry);
  void updateMeshGeometry(uint32_t meshId, const MeshGeometry &geometry);
  void updateMaterialsAtRuntime(const std::vector<MaterialUpdate> &updates);
  void createRayTracingResources();
  void destroyRayTracingResources();
  void flushRayTracingUpdates();
  bool hasRayTracingTlas() const;
  VkAccelerationStructureKHR getRayTracingTlas() const;
  std::optional<TlasDescriptorInfo> getRayTracingTlasDescriptorInfo() const;
  void addLight(const Light &light);
  void clearLights();

  // View configuration.
  void setCameras(std::vector<CameraSpec> cameras);
  const std::vector<CameraSpec> &getCameras() const;
  void setMainCamera(const CameraSpec &camera);
  void setMainCameraClipRange(float clipStart, float clipEnd);
  float getMainCameraClipStart() const;
  float getMainCameraClipEnd() const;

  // Output configuration/query.
  void setTileConfig(TileAtlasConfig config);
  void setRequestedTileAovChannels(TileAovChannelMask channels);
  TileAtlasConfig getTileConfig() const;
  bool isTileMultiviewSupported() const;
  uint32_t getTileMultiviewMaxViewCount() const;
  std::optional<ExportedAovTexture> GetAovTexture(Aov aov) const;
  void setLidarSensors(std::vector<LidarSensorSpec> sensors);
  void setLidarVisualizationConfig(LidarVisualizationConfig config);
  LidarFramePointCloud readLidarPointCloudFrame();
  void setHeightScanSensors(std::vector<HeightScanSensorSpec> sensors);
  void setHeightScanVisualizationConfig(HeightScanVisualizationConfig config);
  HeightScanFrame readHeightScanFrame();

  // Debug helpers.
  std::vector<uint32_t> readObjectIdImage();
  void saveOffscreenColorToFile(const char *filename);

  size_t getInstanceCount() const;
  InstanceInfo getInstance(size_t index) const;
  size_t getMeshSourceCount() const;

private:
  friend class engine::RendererAccess;

  struct Impl;
  std::unique_ptr<Impl> m_impl;
};
