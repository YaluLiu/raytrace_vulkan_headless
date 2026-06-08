#include "core/renderer_internal.hpp"
#include "core/render_resource_lifecycle.hpp"

void Engine::uploadMesh(const MeshGeometry& geometry,
                          std::span<const Material> materials,
                          glm::mat4 transform)
{
  auto& impl = engine::EngineAccess::impl(*this);
  impl.gpuScene.uploadMesh(geometry, materials, transform);
}

void Engine::loadTextureAssets(const std::vector<TextureAsset>& textureAssets)
{
  auto& impl = engine::EngineAccess::impl(*this);
  impl.gpuScene.loadTextureAssets(textureAssets);
}

void Engine::rebuildTextureResourcesAndSceneBindings(const std::vector<TextureAsset>& textureAssets)
{
  auto& impl = engine::EngineAccess::impl(*this);
  vkDeviceWaitIdle(impl.device());
  engine::RenderResourceLifecycle::rebuildTexturesAndSceneBindings(*this, textureAssets);
}

uint32_t Engine::addInstance(const glm::mat4& transform, uint32_t meshIndex, int externalInstanceId)
{
  auto& impl = engine::EngineAccess::impl(*this);
  return impl.gpuScene.addInstance(transform, meshIndex, externalInstanceId);
}

void Engine::updateInstance(uint32_t rendererInstanceIndex, glm::mat4 transform, bool visible, uint32_t traceMask)
{
  auto& impl = engine::EngineAccess::impl(*this);
  impl.gpuScene.updateInstance(rendererInstanceIndex, transform, visible, traceMask);
}

void Engine::updateMeshGeometry(uint32_t meshIndex, const MeshGeometry& geometry)
{
  auto& impl = engine::EngineAccess::impl(*this);
  impl.gpuScene.updateMeshGeometry(meshIndex, geometry);
}

void Engine::updateMaterialsAtRuntime(const std::vector<MaterialUpdate>& updates)
{
  auto& impl = engine::EngineAccess::impl(*this);
  impl.gpuScene.updateMaterialsAtRuntime(updates);
}

void Engine::addLight(const Light& light)
{
  auto& impl = engine::EngineAccess::impl(*this);
  impl.gpuScene.addLight(light);
}

void Engine::clearLights()
{
  auto& impl = engine::EngineAccess::impl(*this);
  impl.gpuScene.clearLights();
}

size_t Engine::getInstanceCount() const
{
  const auto& impl = engine::EngineAccess::impl(*this);
  return impl.gpuScene.getInstanceCount();
}

InstanceInfo Engine::getInstance(size_t index) const
{
  const auto& impl = engine::EngineAccess::impl(*this);
  const auto& instance = impl.gpuScene.getInstance(index);
  return InstanceInfo{
      instance.transform,
      instance.meshIndex,
      0,
      instance.visible,
  };
}

size_t Engine::getMeshSourceCount() const
{
  const auto& impl = engine::EngineAccess::impl(*this);
  return impl.gpuScene.getMeshSourceCount();
}

void Engine::createRayTracingResources()
{
  auto& impl = engine::EngineAccess::impl(*this);
  impl.gpuScene.createRayTracingResources();
}

void Engine::destroyRayTracingResources()
{
  auto& impl = engine::EngineAccess::impl(*this);
  impl.gpuScene.destroyRayTracingResources();
}

void Engine::flushRayTracingUpdates()
{
  auto& impl = engine::EngineAccess::impl(*this);
  impl.gpuScene.flushRayTracingUpdates();
}

bool Engine::hasRayTracingTlas() const
{
  const auto& impl = engine::EngineAccess::impl(*this);
  return impl.gpuScene.hasRayTracingTlas();
}

VkAccelerationStructureKHR Engine::getRayTracingTlas() const
{
  const auto& impl = engine::EngineAccess::impl(*this);
  return impl.gpuScene.getRayTracingTlas();
}

std::optional<TlasDescriptorInfo> Engine::getRayTracingTlasDescriptorInfo() const
{
  const auto& impl = engine::EngineAccess::impl(*this);
  return impl.gpuScene.getRayTracingTlasDescriptorInfo();
}
