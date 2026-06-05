#pragma once

#include "sceneStore.h"

#include <pxr/base/tf/token.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

class Engine;

PXR_NAMESPACE_OPEN_SCOPE

namespace hdrobot {

class GlInteropCache;

class EngineSceneSync final
{
public:
  struct RendererMeshBinding
  {
    int rendererMeshId = -1;
    std::vector<int> rendererInstanceIds;
  };

  void EnsureResources(::Engine& engine, SceneStore& sceneStore, GlInteropCache& glInteropCache);
  void SyncSceneToEngine(::Engine& engine, SceneStore& sceneStore);
  void ApplyFrameRenderTags(::Engine& engine, const TfTokenVector& renderTags);

private:
  RendererMeshBinding* _GetBinding(uint32_t sceneIndex);
  const RendererMeshBinding* _GetBinding(uint32_t sceneIndex) const;

  void _EnsureInitialResources(::Engine& engine, SceneStore& sceneStore);
  void _UploadInitialScene(::Engine& engine, SceneStore& sceneStore);
  void _RefreshTextureAssetsIfNeeded(::Engine& engine,
                                     SceneStore& sceneStore,
                                     GlInteropCache& glInteropCache);
  void _UpdateLights(::Engine& engine, const SceneStore& sceneStore);
  void _UpdateGeometry(::Engine& engine, SceneStore& sceneStore);
  bool _UpdateInstances(::Engine& engine, SceneStore& sceneStore);
  void _UpdateMaterials(::Engine& engine, SceneStore& sceneStore);

  std::unordered_map<uint32_t, RendererMeshBinding> _meshBindings;
  std::vector<MeshRecord> _frameMeshRecords;
  uint64_t _uploadedTextureRegistryVersion = 0;
  bool _engineResourcesCreated = false;
  TfTokenVector _activeRenderTags;
  bool _frameVisibilityDirty = true;
};

} // namespace hdrobot

PXR_NAMESPACE_CLOSE_SCOPE
