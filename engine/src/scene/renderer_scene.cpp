#include "core/renderer_internal.hpp"
#include "core/renderer_resources.hpp"

void Renderer::uploadMesh(const MeshGeometry& geometry,
                          std::span<const Material> materials,
                          glm::mat4 transform)
{
  m_gpuScene.uploadMesh(geometry, materials, transform);
}

void Renderer::loadTextureAssets(const std::vector<TextureAsset>& textureAssets)
{
  m_gpuScene.loadTextureAssets(textureAssets);
}

void Renderer::rebuildTextureResourcesAndSceneBindings(const std::vector<TextureAsset>& textureAssets)
{
  vkDeviceWaitIdle(m_impl->device());

  m_outputController.destroyOutputPipelines();
  m_sceneDescriptors.destroy();

  m_gpuScene.rebuildTextureResources(textureAssets);
  engine::createSceneDescriptors(*this);
  engine::updateSceneDescriptorBindings(*this);
  engine::createOutputPipelines(*this);
}

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

void Renderer::updateMaterialsAtRuntime(const std::vector<MaterialUpdate>& updates)
{
  m_gpuScene.updateMaterialsAtRuntime(updates);
}

void Renderer::addLight(const Light& light)
{
  m_gpuScene.addLight(light);
}

void Renderer::clearLights()
{
  m_gpuScene.clearLights();
}

size_t Renderer::getInstanceCount() const
{
  return m_gpuScene.getInstanceCount();
}

InstanceInfo Renderer::getInstance(size_t index) const
{
  const auto& instance = m_gpuScene.getInstance(index);
  return InstanceInfo{
      instance.transform,
      instance.objIndex,
      0,
      instance.visible,
  };
}

size_t Renderer::getMeshSourceCount() const
{
  return m_gpuScene.getMeshSourceCount();
}

void Renderer::createRayTracingResources()
{
  m_gpuScene.createRayTracingResources();
}

void Renderer::destroyRayTracingResources()
{
  m_gpuScene.destroyRayTracingResources();
}

void Renderer::flushRayTracingUpdates()
{
  m_gpuScene.flushRayTracingUpdates();
}

bool Renderer::hasRayTracingTlas() const
{
  return m_gpuScene.hasRayTracingTlas();
}

VkAccelerationStructureKHR Renderer::getRayTracingTlas() const
{
  return m_gpuScene.getRayTracingTlas();
}

std::optional<TlasDescriptorInfo> Renderer::getRayTracingTlasDescriptorInfo() const
{
  return m_gpuScene.getRayTracingTlasDescriptorInfo();
}
