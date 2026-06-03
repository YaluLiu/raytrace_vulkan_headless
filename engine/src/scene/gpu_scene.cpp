#include "scene/gpu_scene.hpp"

#include <cstddef>
#include <type_traits>

#include "nvvk/commands_vk.hpp"

namespace
{
namespace ShaderAbiChecks
{
static_assert(std::is_standard_layout_v<Material>);
static_assert(std::is_standard_layout_v<WaveFrontMaterial>);
static_assert(sizeof(Material) == sizeof(WaveFrontMaterial));
static_assert(sizeof(MeshVertex) == sizeof(Vertex));
static_assert(offsetof(MeshVertex, pos) == offsetof(Vertex, pos));
static_assert(offsetof(MeshVertex, tangent) == offsetof(Vertex, tangent));
static_assert(offsetof(Material, baseColorFactor) == offsetof(WaveFrontMaterial, baseColorFactor));
static_assert(offsetof(Material, transmissionColorFactor) == offsetof(WaveFrontMaterial, transmissionColorFactor));
static_assert(offsetof(Material, roughnessFactor) == offsetof(WaveFrontMaterial, roughnessFactor));
static_assert(offsetof(Material, transmissionFactor) == offsetof(WaveFrontMaterial, transmissionFactor));
static_assert(offsetof(Material, subsurfaceFactor) == offsetof(WaveFrontMaterial, subsurfaceFactor));
static_assert(offsetof(Material, diffuseTextureId) == offsetof(WaveFrontMaterial, diffuseTextureId));
static_assert(offsetof(Material, normalTextureId) == offsetof(WaveFrontMaterial, normalTextureId));
static_assert(offsetof(Material, subsurfaceTextureId) == offsetof(WaveFrontMaterial, subsurfaceTextureId));
} // namespace ShaderAbiChecks
} // namespace

void GpuScene::setup(VkDevice device,
                     uint32_t graphicsQueueIndex,
                     nvvk::ResourceAllocatorDma& allocator,
                     nvvk::DebugUtil& debug)
{
  m_device = device;
  m_graphicsQueueIndex = graphicsQueueIndex;
  m_alloc = &allocator;
  m_debug = &debug;
  m_meshStore.setup(m_device, m_graphicsQueueIndex, allocator, debug);
  m_textureStore.setup(m_device, m_graphicsQueueIndex, allocator);
  m_lightStore.setup(allocator, debug);
  m_rtScene.setup(m_device, m_graphicsQueueIndex, allocator);
}

void GpuScene::destroy()
{
  m_lightStore.destroy();
  m_rtScene.teardown();
  m_meshStore.destroy();
  m_textureStore.destroy();
  m_instanceStore.clear();
}

uint32_t GpuScene::addInstance(const glm::mat4& transform, uint32_t objIndex, int instanceId)
{
  const uint32_t index = m_instanceStore.addInstance(transform, objIndex, instanceId);
  m_rtScene.markTlasDirty();
  return index;
}

void GpuScene::uploadMesh(const MeshGeometry& geometry,
                          std::span<const Material> materials,
                          glm::mat4 transform)
{
  nvvk::CommandPool cmdBufGet(m_device, m_graphicsQueueIndex);
  VkCommandBuffer cmdBuf = cmdBufGet.createCommandBuffer();
  const uint32_t meshIndex = m_meshStore.uploadMesh(cmdBuf, geometry, materials);
  m_textureStore.uploadTextureResources(cmdBuf, {});
  cmdBufGet.submitAndWait(cmdBuf);
  m_alloc->finalizeAndReleaseStaging();

  addInstance(transform, meshIndex, 0);
  m_rtScene.markBlasDirty(meshIndex);
}

void GpuScene::loadTextureAssets(const std::vector<TextureAsset>& textureAssets)
{
  m_textureStore.loadTextureAssets(textureAssets);
}

void GpuScene::rebuildTextureResources(const std::vector<TextureAsset>& textureAssets)
{
  m_textureStore.rebuildTextureResources(textureAssets);
}

void GpuScene::updateInstance(uint32_t instanceId, glm::mat4 transform, bool visible, uint32_t traceMask)
{
  if(m_instanceStore.updateInstance(instanceId, transform, visible, traceMask))
  {
    m_rtScene.markTlasDirty();
  }
}

void GpuScene::updateMeshGeometry(uint32_t meshId, const MeshGeometry& geometry)
{
  if(m_meshStore.updateGeometry(meshId, geometry))
  {
    m_rtScene.markBlasDirty(meshId);
  }
}

void GpuScene::updateMaterialsAtRuntime(const std::vector<MaterialUpdate>& updates)
{
  m_meshStore.updateMaterialsAtRuntime(updates);
}

void GpuScene::createRayTracingResources()
{
  m_rtScene.build(m_meshStore.buffers(), m_instanceStore.instances());
}

void GpuScene::destroyRayTracingResources()
{
  m_rtScene.destroyAccelerationStructures();
}

void GpuScene::flushRayTracingUpdates()
{
  m_rtScene.flush(m_meshStore.buffers(), m_instanceStore.instances());
}

bool GpuScene::hasRayTracingTlas() const
{
  return m_rtScene.hasTlas();
}

VkAccelerationStructureKHR GpuScene::getRayTracingTlas() const
{
  return m_rtScene.getTlas();
}

std::optional<TlasDescriptorInfo> GpuScene::getRayTracingTlasDescriptorInfo() const
{
  return m_rtScene.getTlasDescriptorInfo();
}

void GpuScene::createObjectDescriptionBuffer()
{
  m_meshStore.createObjectDescriptionBuffer();
}

void GpuScene::addLight(const Light& light)
{
  m_lightStore.addLight(light);
}

void GpuScene::clearLights()
{
  m_lightStore.clearLights();
}

void GpuScene::createLightBuffer()
{
  m_lightStore.createLightBuffer();
}

void GpuScene::updateLightBuffer(const VkCommandBuffer& cmdBuf)
{
  m_lightStore.updateLightBuffer(cmdBuf);
}
