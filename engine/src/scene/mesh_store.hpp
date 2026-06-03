#pragma once

#include <engine/mesh_types.hpp>
#include <engine/renderer_types.hpp>

#include "nvvk/debug_util_vk.hpp"
#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include "scene/scene_types.hpp"
#include "shaders/common/host_device.h"

#include <span>
#include <vector>

#include <vulkan/vulkan_core.h>

struct MeshRecord
{
  MeshGeometry geometry;
  MeshBuffers buffers;
  ObjDesc objectDescription{};
};

class MeshStore final
{
public:
  void setup(VkDevice device,
             uint32_t graphicsQueueIndex,
             nvvk::ResourceAllocatorDma& allocator,
             nvvk::DebugUtil& debug);
  void destroy();

  uint32_t uploadMesh(const VkCommandBuffer& cmdBuf,
                      const MeshGeometry& geometry,
                      std::span<const Material> materials);
  bool updateGeometry(uint32_t meshId, const MeshGeometry& geometry);
  void updateMaterialsAtRuntime(const std::vector<MaterialUpdate>& updates);
  void createObjectDescriptionBuffer();

  std::span<const MeshBuffers> buffers() const { return m_buffers; }
  std::span<const ObjDesc> objectDescriptions() const { return m_objectDescriptions; }
  size_t size() const { return m_records.size(); }
  const nvvk::Buffer& objectDescriptionBuffer() const { return m_objectDescriptionBuffer; }

private:
  void destroyMeshBuffers();

  VkDevice m_device{VK_NULL_HANDLE};
  uint32_t m_graphicsQueueIndex{0};
  nvvk::ResourceAllocatorDma* m_alloc{nullptr};
  nvvk::DebugUtil* m_debug{nullptr};
  std::vector<MeshRecord> m_records;
  std::vector<MeshBuffers> m_buffers;
  std::vector<ObjDesc> m_objectDescriptions;
  nvvk::Buffer m_objectDescriptionBuffer;
};
