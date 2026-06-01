#pragma once

#include <engine/mesh_types.hpp>
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include <engine/renderer_types.hpp>
#include <engine/texture_asset.hpp>
#include "scene/rt_scene.hpp"
#include "scene/scene_types.hpp"
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

  uint32_t addInstance(const glm::mat4& transform, uint32_t objIndex, int instanceId = 0);
  void uploadMesh(const MeshGeometry& geometry,
                  std::span<const Material> materials,
                  glm::mat4 transform = glm::mat4(1));
  void loadTextureAssets(const std::vector<TextureAsset>& textureAssets);
  void rebuildTextureResources(const std::vector<TextureAsset>& textureAssets);
  void uploadTextureResources(const VkCommandBuffer& cmdBuf, const std::vector<TextureAsset>& textureAssets);

  void updateInstance(uint32_t instanceId, glm::mat4 transform, bool visible, uint32_t traceMask);
  void updateMeshGeometry(uint32_t meshId, const MeshGeometry& geometry);
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

  size_t getInstanceCount() const { return m_instances.size(); }
  const SceneInstance& getInstance(size_t index) const { return m_instances[index]; }
  size_t getMeshSourceCount() const { return m_meshGeometries.size(); }
  size_t getLightCount() const { return m_lights.size(); }
  uint32_t getTextureDescriptorCount() const { return static_cast<uint32_t>(m_textures.size()); }

  std::span<const MeshBuffers> getMeshBuffers() const { return m_objModel; }
  std::span<const SceneInstance> getInstances() const { return m_instances; }
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

  std::vector<MeshGeometry> m_meshGeometries;
  std::vector<MeshBuffers> m_objModel;
  std::vector<ObjDesc> m_objDesc;
  std::vector<SceneInstance> m_instances;
  std::vector<int> m_instanceIds;
  std::vector<nvvk::Texture> m_textures;
  std::vector<Light> m_lights;
  RtScene m_rtScene;

  nvvk::Buffer m_bObjDesc;
  nvvk::Buffer m_bLights;
};
