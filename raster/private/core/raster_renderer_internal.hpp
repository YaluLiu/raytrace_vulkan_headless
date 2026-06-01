#pragma once

#include <raster/raster_renderer.hpp>

#include "core/raster_output_controller.hpp"
#include "nvvk/debug_util_vk.hpp"
#include "nvvk/memallocator_dma_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include "runtime/headless_vk.hpp"
#include "scene/raster_gpu_scene.hpp"
#include "scene/raster_scene_descriptors.hpp"
#include "scene/raster_view_uniforms.hpp"

struct RasterRenderer::Impl final : nvvkhl::AppOffline
{
  VkExtent2D& sizeRef() { return m_size; }
  const VkExtent2D& sizeRef() const { return m_size; }
  VkDevice device() const { return m_device; }
  VkQueue queue() const { return m_queue; }
  uint32_t memoryType(uint32_t typeBits, const VkMemoryPropertyFlags& properties) const
  {
    return getMemoryType(typeBits, properties);
  }
  VkCommandBuffer createTempCommandBuffer() { return createTempCmdBuffer(); }
  void submitTempCommandBuffer(VkCommandBuffer cmdBuffer) { submitTempCmdBuffer(cmdBuffer); }

  nvvk::ResourceAllocatorDma alloc;
  nvvk::DebugUtil debug;
  RasterGpuScene gpuScene;
  RasterSceneDescriptors sceneDescriptors;
  RasterViewUniforms viewUniforms;
  RasterOutputController outputController;
};

namespace raster
{
class RasterRendererAccess final
{
public:
  using Impl = RasterRenderer::Impl;

  static Impl& impl(RasterRenderer& renderer) { return *renderer.m_impl; }
  static const Impl& impl(const RasterRenderer& renderer) { return *renderer.m_impl; }
};
}

#define m_alloc (m_impl->alloc)
#define m_debug (m_impl->debug)
#define m_gpuScene (m_impl->gpuScene)
#define m_sceneDescriptors (m_impl->sceneDescriptors)
#define m_viewUniforms (m_impl->viewUniforms)
#define m_outputController (m_impl->outputController)
