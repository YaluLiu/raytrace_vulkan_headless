#include "core/renderer_internal.hpp"

uint32_t Renderer::addInstance(const glm::mat4& transform, uint32_t objIndex, int instanceId)
{
  return m_gpuScene.addInstance(transform, objIndex, instanceId);
}

void Renderer::updateInstance(uint32_t instanceId, glm::mat4 transform, bool visible, uint32_t traceMask)
{
  m_gpuScene.updateInstance(instanceId, transform, visible, traceMask);
}

void Renderer::updateMeshGeometry(uint32_t meshId, const MeshGeometry& geometry)
{
  m_gpuScene.updateMeshGeometry(meshId, geometry);
}
