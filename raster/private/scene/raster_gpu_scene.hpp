#pragma once

#include "ModelLoader.h"
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include <raster/raster_renderer_types.hpp>
#include "scene/raster_rt_scene.hpp"
#include "scene/raster_scene_types.hpp"
#include "shaders/common/host_device.h"

#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

class RasterGpuScene final
{
public:
  using ObjModel = RasterMeshBuffers;
  using ObjInstance = RasterInstance;

  void setup(VkDevice device,
             uint32_t graphicsQueueIndex,
             nvvk::ResourceAllocatorDma& allocator,
             nvvk::DebugUtil& debug);
  void destroy();

  uint32_t addInstance(const glm::mat4& transform, uint32_t objIndex, int instanceId = 0);
  void uploadMeshFromLoader(ModelLoader& loader, glm::mat4 transform = glm::mat4(1));
  void loadTextureAssets(const std::vector<TextureAsset>& textureAssets);
  void rebuildTextureResources(const std::vector<TextureAsset>& textureAssets);
  void uploadTextureResources(const VkCommandBuffer& cmdBuf,
                           const std::vector<std::string>& textures,
                           const std::vector<TextureAsset>& textureAssets);

  void updateInstance(uint32_t instanceId, glm::mat4 transform, bool visible, uint32_t traceMask);
  void updateMeshGeometry(uint32_t meshId);
  void updateMaterialsAtRuntime(const std::vector<RasterMaterialUpdate>& updates);
  void createRayTracingResources();
  void destroyRayTracingResources();
  void flushRayTracingUpdates();
  bool hasRayTracingTlas() const;
  VkAccelerationStructureKHR getRayTracingTlas() const;
  std::optional<RasterTlasDescriptorInfo> getRayTracingTlasDescriptorInfo() const;

  void createObjectDescriptionBuffer();
  void addLight(const Light& light);
  void clearLights();
  void createLightBuffer();
  void updateLightBuffer(const VkCommandBuffer& cmdBuf);

  size_t getInstanceCount() const { return m_instances.size(); }
  const ObjInstance& getInstance(size_t index) const { return m_instances[index]; }
  ModelLoader& getMutableMeshSourceLoader(size_t meshId) { return m_loaders[meshId]; }
  size_t getMeshSourceCount() const { return m_loaders.size(); }
  size_t getLightCount() const { return m_lights.size(); }
  uint32_t getTextureDescriptorCount() const { return static_cast<uint32_t>(m_textures.size()); }

  std::span<const ObjModel> getMeshBuffers() const { return m_objModel; }
  std::span<const ObjInstance> getInstances() const { return m_instances; }
  std::span<const int> getInstanceIds() const { return m_instanceIds; }
  std::span<const nvvk::Texture> getTextures() const { return m_textures; }
  const nvvk::Buffer& getObjectDescriptionBuffer() const { return m_bObjDesc; }
  const nvvk::Buffer& getLightBuffer() const { return m_bLights; }

private:
  void destroyTextures();
  void destroyMeshBuffers();

  VkDevice m_device{VK_NULL_HANDLE};
  uint32_t m_graphicsQueueIndex{0};
  nvvk::ResourceAllocatorDma* m_alloc{nullptr};
  nvvk::DebugUtil* m_debug{nullptr};

  std::vector<ModelLoader> m_loaders;
  std::vector<ObjModel> m_objModel;
  std::vector<ObjDesc> m_objDesc;
  std::vector<ObjInstance> m_instances;
  std::vector<int> m_instanceIds;
  std::vector<nvvk::Texture> m_textures;
  std::vector<Light> m_lights;
  RasterRtScene m_rtScene;

  nvvk::Buffer m_bObjDesc;
  nvvk::Buffer m_bLights;
};
