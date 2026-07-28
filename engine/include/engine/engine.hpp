#pragma once

#include <engine/aov_texture.hpp>
#include <engine/mesh_types.hpp>
#include <engine/output_config.hpp>
#include <engine/renderer_types.hpp>
#include <engine/tile_depth_types.hpp>
#include <engine/texture_asset.hpp>

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <vulkan/vulkan_core.h>

namespace engine
{
class EngineImplAccess;
}

class Engine
{
public:
  Engine();
  ~Engine();
  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;
  Engine(Engine&&) = delete;
  Engine& operator=(Engine&&) = delete;

  // Runtime lifecycle for the engine-owned Vulkan context.
  void setup(int width = 1280, int height = 720);
  void setPluginSearchRoot(std::string pluginSearchRoot);
  void resize(int width, int height);
  void createRenderResources();
  void render();
  void cleanup();
  void saveFrame(std::string outputImagePath = "headless.png");

  // Lifecycle for callers that inject an existing Vulkan context.
  void setup(const VkInstance& instance, const VkDevice& device, const VkPhysicalDevice& physicalDevice,
             uint32_t queueFamily);
  void create(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice, uint32_t queueFamily,
              VkExtent2D size);
  void destroy();
  VkDevice getDevice() const;
  const std::vector<VkCommandBuffer>& getCommandBuffers() const;
  uint32_t getCurFrame() const;
  void submitCurrentCommandBufferAndWait();
  void onResize(int width, int height);

  // Scene upload/update facade.
  void uploadMesh(const MeshGeometry& geometry,
                  std::span<const Material> materials,
                  glm::mat4 transform = glm::mat4(1));
  void loadTextureAssets(const std::vector<TextureAsset>& textureAssets);
  void rebuildTextureResourcesAndSceneBindings(const std::vector<TextureAsset>& textureAssets);
  uint32_t addInstance(const glm::mat4& transform, uint32_t meshIndex, int externalInstanceId = 0);
  void updateInstance(uint32_t rendererInstanceIndex,
                      glm::mat4 transform,
                      bool visible,
                      uint32_t traceMask = kTraceMaskDefaultGeometry);
  void updateMeshGeometry(uint32_t meshIndex, const MeshGeometry& geometry);
  void updateMaterialsAtRuntime(const std::vector<MaterialUpdate>& updates);
  void createRayTracingResources();
  void destroyRayTracingResources();
  void flushRayTracingUpdates();
  bool hasRayTracingTlas() const;
  VkAccelerationStructureKHR getRayTracingTlas() const;
  std::optional<TlasDescriptorInfo> getRayTracingTlasDescriptorInfo() const;
  void addLight(const Light& light);
  void clearLights();

  // View configuration.
  void setCameras(std::vector<CameraSpec> cameras);
  const std::vector<CameraSpec>& getCameras() const;
  void setMainCamera(const CameraSpec& camera);
  void setMainCameraClipRange(float clipStart, float clipEnd);
  float getMainCameraClipStart() const;
  float getMainCameraClipEnd() const;

  // Output configuration/query.
  void configureOutputs(RendererOutputConfig config);
  TileAtlasConfig getTileConfig() const;
  bool isTileMultiviewSupported() const;
  uint32_t getTileMultiviewMaxViewCount() const;
  std::optional<ExportedAovTexture> GetAovTexture(Aov aov) const;
  TileDepthFrame readTileDepthFrame();
  LidarFramePointCloud readLidarPointCloudFrame();
  HeightScanFrame readHeightScanFrame();

  // Debug helpers.
  std::vector<uint32_t> readObjectIdImage();
  void saveOffscreenColorToFile(const char* filename);

  size_t getInstanceCount() const;
  InstanceInfo getInstance(size_t index) const;
  size_t getMeshSourceCount() const;

private:
  friend class engine::EngineImplAccess;

  struct Impl;
  std::unique_ptr<Impl> m_impl;

  void setupContext();
};
