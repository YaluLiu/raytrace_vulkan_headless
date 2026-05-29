#include "core/raster_renderer_internal.hpp"

uint32_t RasterRenderer::addInstance(const glm::mat4& transform, uint32_t objIndex, int instanceId)
{
  return m_gpuScene.addInstance(transform, objIndex, instanceId);
}

void RasterRenderer::updateInstance(uint32_t instanceId, glm::mat4 transform, bool visible, uint32_t traceMask)
{
  m_gpuScene.updateInstance(instanceId, transform, visible, traceMask);
}

void RasterRenderer::updateMeshGeometry(uint32_t meshId, const RasterMeshGeometry& geometry)
{
  m_gpuScene.updateMeshGeometry(meshId, geometry);
}
