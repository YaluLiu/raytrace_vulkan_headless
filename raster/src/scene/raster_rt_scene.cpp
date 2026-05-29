#include "scene/raster_rt_scene.hpp"

#include "nvvk/acceleration_structures.hpp"
#include "nvvk/buffers_vk.hpp"
#include <raster/mesh_types.hpp>

#include <limits>

namespace
{
constexpr uint32_t kInvalidBlasIndex = std::numeric_limits<uint32_t>::max();

VkBuildAccelerationStructureFlagsKHR BlasBuildFlags()
{
  return VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR |
         VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR;
}

VkBuildAccelerationStructureFlagsKHR TlasBuildFlags()
{
  return VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR |
         VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
}
} // namespace

void RasterRtScene::setup(VkDevice device, uint32_t graphicsQueueIndex, nvvk::ResourceAllocatorDma& allocator)
{
  m_device = device;
  m_graphicsQueueIndex = graphicsQueueIndex;
  m_alloc = &allocator;
  m_builder.setup(m_device, m_alloc, m_graphicsQueueIndex);
  m_setup = true;
}

void RasterRtScene::destroy()
{
  if(!m_setup && !m_built && !m_hasBlas)
  {
    return;
  }
  m_builder.destroy();
  m_blasInputs.clear();
  m_meshToBlas.clear();
  m_tlasInstanceCount = 0;
  m_setup = false;
  m_built = false;
  m_hasBlas = false;
  m_fullRebuildDirty = false;
  m_tlasDirty = false;
  m_device = VK_NULL_HANDLE;
  m_graphicsQueueIndex = 0;
  m_alloc = nullptr;
}

void RasterRtScene::build(std::span<const RasterMeshBuffers> meshes, std::span<const RasterInstance> instances)
{
  if(!m_setup)
  {
    return;
  }
  rebuild(meshes, instances);
}

void RasterRtScene::markBlasDirty(uint32_t)
{
  if(m_built)
  {
    m_fullRebuildDirty = true;
  }
}

void RasterRtScene::markTlasDirty()
{
  if(m_built)
  {
    m_tlasDirty = true;
  }
}

void RasterRtScene::flush(std::span<const RasterMeshBuffers> meshes, std::span<const RasterInstance> instances)
{
  if(!m_setup)
  {
    return;
  }

  if(!m_built || m_fullRebuildDirty)
  {
    rebuild(meshes, instances);
    return;
  }

  if(m_tlasDirty)
  {
    rebuildTlas(meshes, instances, true);
    m_tlasDirty = false;
  }
}

bool RasterRtScene::hasTlas() const
{
  return getTlas() != VK_NULL_HANDLE;
}

VkAccelerationStructureKHR RasterRtScene::getTlas() const
{
  return m_built ? m_builder.getAccelerationStructure() : VK_NULL_HANDLE;
}

std::optional<RasterTlasDescriptorInfo> RasterRtScene::getTlasDescriptorInfo() const
{
  if(!hasTlas())
  {
    return std::nullopt;
  }
  return RasterTlasDescriptorInfo{getTlas()};
}

bool RasterRtScene::isMeshTraceable(const RasterMeshBuffers& mesh) const
{
  return mesh.vertexBuffer.buffer != VK_NULL_HANDLE && mesh.indexBuffer.buffer != VK_NULL_HANDLE &&
         mesh.nbVertices > 0 && mesh.nbIndices >= 3;
}

RasterRtScene::BlasInput RasterRtScene::makeBlasInput(const RasterMeshBuffers& mesh) const
{
  BlasInput input;
  if(!isMeshTraceable(mesh))
  {
    return input;
  }

  VkAccelerationStructureGeometryTrianglesDataKHR triangles{
      VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
  triangles.vertexFormat = VK_FORMAT_R32G32B32_SFLOAT;
  triangles.vertexData.deviceAddress = nvvk::getBufferDeviceAddress(m_device, mesh.vertexBuffer.buffer);
  triangles.vertexStride = sizeof(RasterVertex);
  triangles.indexType = VK_INDEX_TYPE_UINT32;
  triangles.indexData.deviceAddress = nvvk::getBufferDeviceAddress(m_device, mesh.indexBuffer.buffer);
  triangles.maxVertex = mesh.nbVertices - 1;

  VkAccelerationStructureGeometryKHR geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
  geometry.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  geometry.flags = VK_GEOMETRY_OPAQUE_BIT_KHR;
  geometry.geometry.triangles = triangles;

  VkAccelerationStructureBuildRangeInfoKHR range{};
  range.firstVertex = 0;
  range.primitiveCount = mesh.nbIndices / 3;
  range.primitiveOffset = 0;
  range.transformOffset = 0;

  input.asGeometry.emplace_back(geometry);
  input.asBuildOffsetInfo.emplace_back(range);
  return input;
}

void RasterRtScene::rebuild(std::span<const RasterMeshBuffers> meshes, std::span<const RasterInstance> instances)
{
  if(m_device == VK_NULL_HANDLE || m_alloc == nullptr)
  {
    return;
  }

  m_builder.destroy();
  m_builder.setup(m_device, m_alloc, m_graphicsQueueIndex);
  m_blasInputs.clear();
  m_meshToBlas.assign(meshes.size(), kInvalidBlasIndex);
  m_tlasInstanceCount = 0;
  m_hasBlas = false;
  m_built = false;

  for(size_t meshIndex = 0; meshIndex < meshes.size(); ++meshIndex)
  {
    if(!isMeshTraceable(meshes[meshIndex]))
    {
      continue;
    }
    m_meshToBlas[meshIndex] = static_cast<uint32_t>(m_blasInputs.size());
    m_blasInputs.emplace_back(makeBlasInput(meshes[meshIndex]));
  }

  if(m_blasInputs.empty())
  {
    m_fullRebuildDirty = false;
    m_tlasDirty = false;
    return;
  }

  m_builder.buildBlas(m_blasInputs, BlasBuildFlags());
  m_hasBlas = true;
  rebuildTlas(meshes, instances, false);
  m_fullRebuildDirty = false;
  m_tlasDirty = false;
}

std::vector<VkAccelerationStructureInstanceKHR> RasterRtScene::makeTlasInstances(
    std::span<const RasterMeshBuffers> meshes,
    std::span<const RasterInstance> instances)
{
  std::vector<VkAccelerationStructureInstanceKHR> tlasInstances;
  if(!m_hasBlas)
  {
    return tlasInstances;
  }

  tlasInstances.reserve(instances.size());
  for(size_t instanceIndex = 0; instanceIndex < instances.size(); ++instanceIndex)
  {
    const RasterInstance& instance = instances[instanceIndex];
    const bool meshIndexValid = instance.objIndex < meshes.size() && instance.objIndex < m_meshToBlas.size();
    if(!meshIndexValid)
    {
      continue;
    }

    const uint32_t blasIndex = m_meshToBlas[instance.objIndex];
    if(blasIndex == kInvalidBlasIndex)
    {
      continue;
    }

    VkAccelerationStructureInstanceKHR rayInstance{};
    rayInstance.transform = nvvk::toTransformMatrixKHR(instance.transform);
    rayInstance.instanceCustomIndex = static_cast<uint32_t>(instanceIndex);
    rayInstance.accelerationStructureReference = m_builder.getBlasDeviceAddress(blasIndex);
    rayInstance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    rayInstance.mask = instance.visible ? instance.traceMask : kRasterTraceMaskInvisible;
    rayInstance.instanceShaderBindingTableRecordOffset = 0;
    tlasInstances.emplace_back(rayInstance);
  }

  return tlasInstances;
}

void RasterRtScene::rebuildTlas(std::span<const RasterMeshBuffers> meshes,
                                std::span<const RasterInstance> instances,
                                bool update)
{
  std::vector<VkAccelerationStructureInstanceKHR> tlasInstances = makeTlasInstances(meshes, instances);
  if(tlasInstances.empty())
  {
    m_tlasInstanceCount = 0;
    m_built = false;
    return;
  }

  if(update && tlasInstances.size() != m_tlasInstanceCount)
  {
    rebuild(meshes, instances);
    return;
  }

  m_builder.buildTlas(tlasInstances, TlasBuildFlags(), update);
  m_tlasInstanceCount = static_cast<uint32_t>(tlasInstances.size());
  m_built = true;
}
