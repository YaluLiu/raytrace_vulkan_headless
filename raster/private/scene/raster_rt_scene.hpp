#pragma once

#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/raytraceKHR_vk.hpp"
#include <raster/raster_renderer_types.hpp>
#include "scene/raster_scene_types.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <vulkan/vulkan_core.h>

class RasterRtScene final
{
public:
  void setup(VkDevice device, uint32_t graphicsQueueIndex, nvvk::ResourceAllocatorDma& allocator);
  void destroy();

  void build(std::span<const RasterMeshBuffers> meshes, std::span<const RasterInstance> instances);
  void markBlasDirty(uint32_t meshId);
  void markTlasDirty();
  void flush(std::span<const RasterMeshBuffers> meshes, std::span<const RasterInstance> instances);

  bool hasTlas() const;
  VkAccelerationStructureKHR getTlas() const;
  std::optional<RasterTlasDescriptorInfo> getTlasDescriptorInfo() const;

private:
  using BlasInput = nvvk::RaytracingBuilderKHR::BlasInput;

  BlasInput makeBlasInput(const RasterMeshBuffers& mesh) const;
  bool isMeshTraceable(const RasterMeshBuffers& mesh) const;
  void rebuild(std::span<const RasterMeshBuffers> meshes, std::span<const RasterInstance> instances);
  void rebuildTlas(std::span<const RasterMeshBuffers> meshes, std::span<const RasterInstance> instances, bool update);
  std::vector<VkAccelerationStructureInstanceKHR> makeTlasInstances(std::span<const RasterMeshBuffers> meshes,
                                                                    std::span<const RasterInstance> instances);

  VkDevice m_device{VK_NULL_HANDLE};
  uint32_t m_graphicsQueueIndex{0};
  nvvk::ResourceAllocatorDma* m_alloc{nullptr};
  nvvk::RaytracingBuilderKHR m_builder;
  std::vector<BlasInput> m_blasInputs;
  std::vector<uint32_t> m_meshToBlas;
  uint32_t m_tlasInstanceCount{0};
  bool m_setup{false};
  bool m_built{false};
  bool m_hasBlas{false};
  bool m_fullRebuildDirty{false};
  bool m_tlasDirty{false};
};
