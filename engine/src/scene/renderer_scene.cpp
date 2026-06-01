#include "core/renderer_internal.hpp"
#include "core/renderer_resources.hpp"

void Engine::uploadMesh(const MeshGeometry& geometry,
                          std::span<const Material> materials,
                          glm::mat4 transform)
{
  m_gpuScene.uploadMesh(geometry, materials, transform);
}

void Engine::loadTextureAssets(const std::vector<TextureAsset>& textureAssets)
{
  m_gpuScene.loadTextureAssets(textureAssets);
}

void Engine::rebuildTextureResourcesAndSceneBindings(const std::vector<TextureAsset>& textureAssets)
{
  vkDeviceWaitIdle(m_impl->device());

  m_outputController.destroyOutputPipelines();
  m_sceneDescriptors.destroy();

  m_gpuScene.rebuildTextureResources(textureAssets);
  engine::createSceneDescriptors(*this);
  engine::updateSceneDescriptorBindings(*this);
  engine::createOutputPipelines(*this);
}

uint32_t Engine::addInstance(const glm::mat4& transform, uint32_t objIndex, int instanceId)
{
  return m_gpuScene.addInstance(transform, objIndex, instanceId);
}

void Engine::updateInstance(uint32_t instanceId, glm::mat4 transform, bool visible, uint32_t traceMask)
{
  m_gpuScene.updateInstance(instanceId, transform, visible, traceMask);
}

void Engine::updateMeshGeometry(uint32_t meshId, const MeshGeometry& geometry)
{
  m_gpuScene.updateMeshGeometry(meshId, geometry);
}

void Engine::updateMaterialsAtRuntime(const std::vector<MaterialUpdate>& updates)
{
  m_gpuScene.updateMaterialsAtRuntime(updates);
}

void Engine::addLight(const Light& light)
{
  m_gpuScene.addLight(light);
}

void Engine::clearLights()
{
  m_gpuScene.clearLights();
}

size_t Engine::getInstanceCount() const
{
  return m_gpuScene.getInstanceCount();
}

InstanceInfo Engine::getInstance(size_t index) const
{
  const auto& instance = m_gpuScene.getInstance(index);
  return InstanceInfo{
      instance.transform,
      instance.objIndex,
      0,
      instance.visible,
  };
}

size_t Engine::getMeshSourceCount() const
{
  return m_gpuScene.getMeshSourceCount();
}

void Engine::createRayTracingResources()
{
  m_gpuScene.createRayTracingResources();
}

void Engine::destroyRayTracingResources()
{
  m_gpuScene.destroyRayTracingResources();
}

void Engine::flushRayTracingUpdates()
{
  m_gpuScene.flushRayTracingUpdates();
}

bool Engine::hasRayTracingTlas() const
{
  return m_gpuScene.hasRayTracingTlas();
}

VkAccelerationStructureKHR Engine::getRayTracingTlas() const
{
  return m_gpuScene.getRayTracingTlas();
}

std::optional<TlasDescriptorInfo> Engine::getRayTracingTlasDescriptorInfo() const
{
  return m_gpuScene.getRayTracingTlasDescriptorInfo();
}
