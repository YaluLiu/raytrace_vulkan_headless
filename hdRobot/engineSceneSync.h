#pragma once

#include "sceneStore.h"

#include <pxr/base/tf/token.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

class Engine;
class HdRobotGlInteropCache;

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotEngineSceneSync final
{
public:
  struct RendererMeshBinding
  {
    int rendererMeshId = -1;
    std::vector<int> rendererInstanceIds;
  };

  void EnsureResources(::Engine& engine, HdRobotSceneStore& sceneStore, ::HdRobotGlInteropCache& glInteropCache);
  void SyncSceneToEngine(::Engine& engine, HdRobotSceneStore& sceneStore);
  void ApplyFrameRenderTags(::Engine& engine, const TfTokenVector& renderTags);

private:
  RendererMeshBinding* _GetBinding(uint32_t sceneIndex);
  const RendererMeshBinding* _GetBinding(uint32_t sceneIndex) const;

  void _EnsureInitialResources(::Engine& engine, HdRobotSceneStore& sceneStore);
  void _UploadInitialScene(::Engine& engine, HdRobotSceneStore& sceneStore);
  void _RefreshTextureAssetsIfNeeded(::Engine& engine,
                                     HdRobotSceneStore& sceneStore,
                                     ::HdRobotGlInteropCache& glInteropCache);
  void _UpdateLights(::Engine& engine, const HdRobotSceneStore& sceneStore);
  void _UpdateGeometry(::Engine& engine, HdRobotSceneStore& sceneStore);
  bool _UpdateInstances(::Engine& engine, HdRobotSceneStore& sceneStore);
  void _UpdateMaterials(::Engine& engine, HdRobotSceneStore& sceneStore);

  std::unordered_map<uint32_t, RendererMeshBinding> _meshBindings;
  std::vector<HdRobotMeshRecord> _frameMeshRecords;
  uint64_t _uploadedTextureRegistryVersion = 0;
  bool _engineResourcesCreated = false;
  TfTokenVector _activeRenderTags;
  bool _frameVisibilityDirty = true;
};

PXR_NAMESPACE_CLOSE_SCOPE
