#pragma once

#include <engine/mesh_types.hpp>
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include <engine/renderer_types.hpp>
#include <engine/texture_asset.hpp>
#include "scene/instance_store.hpp"
#include "scene/light_store.hpp"
#include "scene/mesh_store.hpp"
#include "scene/rt_scene.hpp"
#include "scene/scene_types.hpp"
#include "scene/texture_store.hpp"
#include "shaders/common/host_device.h"

#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>
#include <vulkan/vulkan_core.h>

class GpuScene final
{
public:
  void setup(VkDevice device,
             uint32_t graphicsQueueIndex,
             nvvk::ResourceAllocatorDma& allocator,
             nvvk::DebugUtil& debug);
  void destroy();

  uint32_t addInstance(const glm::mat4& transform, uint32_t meshIndex, int externalInstanceId = 0);
  void uploadMesh(const MeshGeometry& geometry,
                  std::span<const Material> materials,
                  glm::mat4 transform = glm::mat4(1));
  void loadTextureAssets(const std::vector<TextureAsset>& textureAssets);
  void rebuildTextureResources(const std::vector<TextureAsset>& textureAssets);

  void updateInstance(uint32_t rendererInstanceIndex, glm::mat4 transform, bool visible, uint32_t traceMask);
  void updateMeshGeometry(uint32_t meshIndex, const MeshGeometry& geometry);
  void updateMaterialsAtRuntime(const std::vector<MaterialUpdate>& updates);
  void createRayTracingResources();
  void destroyRayTracingResources();
  void flushRayTracingUpdates();
  bool hasRayTracingTlas() const;
  VkAccelerationStructureKHR getRayTracingTlas() const;
  std::optional<TlasDescriptorInfo> getRayTracingTlasDescriptorInfo() const;

  void createObjectDescriptionBuffer();
  void addLight(const Light& light);
  void clearLights();
  void createLightBuffer();
  void updateLightBuffer(const VkCommandBuffer& cmdBuf);

  size_t getInstanceCount() const { return m_instanceStore.size(); }
  const SceneInstance& getInstance(size_t index) const { return m_instanceStore.get(index); }
  size_t getMeshSourceCount() const { return m_meshStore.size(); }
  size_t getLightCount() const { return m_lightStore.size(); }
  uint32_t getTextureDescriptorCount() const { return m_textureStore.descriptorCount(); }

  std::span<const MeshBuffers> getMeshBuffers() const { return m_meshStore.buffers(); }
  std::span<const SceneInstance> getInstances() const { return m_instanceStore.instances(); }
  std::span<const int> getExternalInstanceIds() const { return m_instanceStore.externalInstanceIds(); }
  std::span<const nvvk::Texture> getTextures() const { return m_textureStore.textures(); }
  const nvvk::Buffer& getObjectDescriptionBuffer() const { return m_meshStore.objectDescriptionBuffer(); }
  const nvvk::Buffer& getLightBuffer() const { return m_lightStore.buffer(); }

private:
  VkDevice m_device{VK_NULL_HANDLE};
  uint32_t m_graphicsQueueIndex{0};
  nvvk::ResourceAllocatorDma* m_alloc{nullptr};
  nvvk::DebugUtil* m_debug{nullptr};

  MeshStore m_meshStore;
  TextureStore m_textureStore;
  InstanceStore m_instanceStore;
  LightStore m_lightStore;
  RtScene m_rtScene;
};
