#include "engineSceneSync.h"

#include "glInteropCache.h"
#include "hydraTextureAssetExport.h"
#include "passBridgeConversions.h"
#include "tokens.h"

#include <engine/engine.hpp>

#include <algorithm>
#include <set>
#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

namespace hdrobot {

namespace
{
TfTokenVector NormalizeRenderTags(TfTokenVector tags)
{
  std::sort(tags.begin(), tags.end(), [](const TfToken& a, const TfToken& b) { return a.GetString() < b.GetString(); });
  tags.erase(std::unique(tags.begin(), tags.end()), tags.end());
  return tags;
}

bool RenderTagsEqual(const TfTokenVector& lhs, const TfTokenVector& rhs)
{
  if(lhs.size() != rhs.size())
  {
    return false;
  }
  for(size_t i = 0; i < lhs.size(); ++i)
  {
    if(lhs[i] != rhs[i])
    {
      return false;
    }
  }
  return true;
}

bool IsMeshRenderTagMatched(const HydraMesh& mesh, const TfTokenVector& renderTags)
{
  if(renderTags.empty())
  {
    return true;
  }
  return std::find(renderTags.begin(), renderTags.end(), mesh.renderTag) != renderTags.end();
}

std::vector<::Material> CollectMaterialsForMesh(const HydraMesh& mesh,
                                                const std::vector<MaterialRecord>& materials)
{
  std::vector<::Material> materialsForUpload;
  materialsForUpload.reserve(mesh.scene_mat_ids.size());
  for(const int matId : mesh.scene_mat_ids)
  {
    const size_t materialIndex = matId >= 0 ? static_cast<size_t>(matId) : 0;
    const size_t fallbackIndex = materials.empty() ? materialIndex : 0;
    const size_t resolvedIndex = materialIndex < materials.size() ? materialIndex : fallbackIndex;
    if(resolvedIndex < materials.size())
    {
      materialsForUpload.emplace_back(ToMaterial(materials[resolvedIndex].data));
    }
  }
  if(materialsForUpload.empty())
  {
    HydraMaterial fallback;
    fallback.set_default();
    materialsForUpload.emplace_back(ToMaterial(fallback));
  }
  return materialsForUpload;
}

EngineSceneSync::RendererMeshBinding UploadMeshToRenderer(
    ::Engine& engine,
    const HydraMesh& mesh,
    const std::vector<MaterialRecord>& materials)
{
  EngineSceneSync::RendererMeshBinding binding;
  binding.rendererMeshIndex = static_cast<int>(engine.getMeshSourceCount());

  ::MeshGeometry geometry;
  ConvertHydraMeshToGeometry(mesh, geometry);
  std::vector<::Material> materialsForUpload = CollectMaterialsForMesh(mesh, materials);
  engine.uploadMesh(geometry, materialsForUpload);

  const size_t firstRendererInstanceIndex = engine.getInstanceCount() - 1;
  const auto instance = engine.getInstance(firstRendererInstanceIndex);
  const size_t authoredInstanceCount = mesh.instanceTransforms.size();
  const size_t rendererInstanceSlotCount = std::max<size_t>(authoredInstanceCount, 1);
  for(size_t i = 1; i < rendererInstanceSlotCount; ++i)
  {
    engine.addInstance(instance.transform, instance.meshIndex, static_cast<int>(i));
  }
  binding.rendererInstanceIndices.reserve(rendererInstanceSlotCount);
  for(size_t i = 0; i < rendererInstanceSlotCount; ++i)
  {
    binding.rendererInstanceIndices.push_back(static_cast<int>(i + firstRendererInstanceIndex));
  }
  return binding;
}

bool EnsureRendererInstanceSlots(::Engine& engine, EngineSceneSync::RendererMeshBinding& binding, const HydraMesh& mesh)
{
  if(binding.rendererInstanceIndices.empty())
  {
    return false;
  }

  const size_t requiredSlotCount = std::max<size_t>(mesh.instanceTransforms.size(), 1);
  if(binding.rendererInstanceIndices.size() >= requiredSlotCount)
  {
    return false;
  }

  const InstanceInfo baseInstance = engine.getInstance(static_cast<size_t>(binding.rendererInstanceIndices.front()));
  for(size_t i = binding.rendererInstanceIndices.size(); i < requiredSlotCount; ++i)
  {
    const uint32_t rendererInstanceIndex =
        engine.addInstance(baseInstance.transform, baseInstance.meshIndex, static_cast<int>(i));
    binding.rendererInstanceIndices.push_back(static_cast<int>(rendererInstanceIndex));
  }
  return true;
}

void UpdateRendererInstances(::Engine& engine,
                             const MeshRecord& record,
                             const std::vector<int>& rendererInstanceIndices,
                             const TfTokenVector& renderTags)
{
  const HydraMesh& mesh = record.data;
  const bool tagMatched = IsMeshRenderTagMatched(mesh, renderTags);
  const bool meshVisible = record.active && mesh.visible && mesh.valid && tagMatched;
  const uint32_t traceMask = mesh.traceRole == HdRobotTraceRoleTokens->ground
                                 ? kTraceMaskGround
                                 : kTraceMaskDefaultGeometry;

  for(size_t instanceIndex = 0; instanceIndex < rendererInstanceIndices.size(); ++instanceIndex)
  {
    const int rendererInstanceIndex = rendererInstanceIndices[instanceIndex];
    if(rendererInstanceIndex < 0)
    {
      continue;
    }

    const bool instanceValid = mesh.hasInstances && (instanceIndex < mesh.instanceTransforms.size());
    const bool visible = meshVisible && instanceValid;
    glm::mat4 transform = glm::transpose(mesh.transform);
    if(instanceValid)
    {
      transform = glm::transpose(mesh.transform * mesh.instanceTransforms[instanceIndex]);
    }
    engine.updateInstance(static_cast<uint32_t>(rendererInstanceIndex), transform, visible, traceMask);
  }
}
} // namespace

EngineSceneSync::RendererMeshBinding* EngineSceneSync::_GetBinding(uint32_t sceneIndex)
{
  const auto bindingIt = _meshBindings.find(sceneIndex);
  return bindingIt == _meshBindings.end() ? nullptr : &bindingIt->second;
}

const EngineSceneSync::RendererMeshBinding* EngineSceneSync::_GetBinding(uint32_t sceneIndex) const
{
  const auto bindingIt = _meshBindings.find(sceneIndex);
  return bindingIt == _meshBindings.end() ? nullptr : &bindingIt->second;
}

void EngineSceneSync::EnsureResources(::Engine& engine,
                                             SceneStore& sceneStore,
                                             GlInteropCache& glInteropCache)
{
  _EnsureInitialResources(engine, sceneStore);
  _RefreshTextureAssetsIfNeeded(engine, sceneStore, glInteropCache);
}

void EngineSceneSync::SyncSceneToEngine(::Engine& engine, SceneStore& sceneStore)
{
  _UpdateLights(engine, sceneStore);
  _UpdateGeometry(engine, sceneStore);
  if(_UpdateInstances(engine, sceneStore))
  {
    _frameVisibilityDirty = true;
  }
  _UpdateMaterials(engine, sceneStore);

  _frameMeshRecords = sceneStore.GetMeshRecordsSnapshot();
}

void EngineSceneSync::ApplyFrameRenderTags(::Engine& engine, const TfTokenVector& renderTags)
{
  TfTokenVector normalizedRenderTags = NormalizeRenderTags(renderTags);
  const bool tagsChanged = !RenderTagsEqual(_activeRenderTags, normalizedRenderTags);
  if(!tagsChanged && !_frameVisibilityDirty)
  {
    return;
  }

  _activeRenderTags = std::move(normalizedRenderTags);
  for(const MeshRecord& record : _frameMeshRecords)
  {
    const RendererMeshBinding* binding = _GetBinding(record.sceneIndex);
    if(binding == nullptr)
    {
      continue;
    }
    UpdateRendererInstances(engine, record, binding->rendererInstanceIndices, _activeRenderTags);
  }
  _frameVisibilityDirty = false;
}

void EngineSceneSync::_EnsureInitialResources(::Engine& engine, SceneStore& sceneStore)
{
  if(_engineResourcesCreated)
  {
    return;
  }

  engine.loadTextureAssets(ExportRegisteredTextures(sceneStore.GetTextureAssetsSnapshot()));
  _uploadedTextureRegistryVersion = sceneStore.GetTextureRegistryVersion();
  _UploadInitialScene(engine, sceneStore);
  engine.createRenderResources();
  _engineResourcesCreated = true;
  _frameVisibilityDirty = true;
}

void EngineSceneSync::_UploadInitialScene(::Engine& engine, SceneStore& sceneStore)
{
  std::vector<MeshRecord> meshRecords = sceneStore.GetMeshRecordsSnapshot();
  const std::vector<MaterialRecord> materialRecords = sceneStore.GetMaterialRecordsSnapshot();
  for(const MeshRecord& record : meshRecords)
  {
    if(!record.active || !record.data.valid)
    {
      continue;
    }
    _meshBindings[record.sceneIndex] = UploadMeshToRenderer(engine, record.data, materialRecords);
  }
}

void EngineSceneSync::_RefreshTextureAssetsIfNeeded(::Engine& engine,
                                                           SceneStore& sceneStore,
                                                           GlInteropCache& glInteropCache)
{
  const uint64_t textureRegistryVersion = sceneStore.GetTextureRegistryVersion();
  if(textureRegistryVersion == _uploadedTextureRegistryVersion)
  {
    return;
  }

  glInteropCache.Clear();
  engine.rebuildTextureResourcesAndSceneBindings(ExportRegisteredTextures(sceneStore.GetTextureAssetsSnapshot()));
  _uploadedTextureRegistryVersion = textureRegistryVersion;
}

void EngineSceneSync::_UpdateLights(::Engine& engine, const SceneStore& sceneStore)
{
  engine.clearLights();

  for(const HydraLight& curLight : sceneStore.GetActiveLightSnapshot())
  {
    engine.addLight(ToLight(curLight));
  }
}

void EngineSceneSync::_UpdateGeometry(::Engine& engine, SceneStore& sceneStore)
{
  const std::vector<MaterialRecord> materialRecords = sceneStore.GetMaterialRecordsSnapshot();
  for(const MeshRecord& record : sceneStore.ConsumeDirtyMeshGeometry())
  {
    if(!record.active || !record.data.valid)
    {
      continue;
    }

    RendererMeshBinding* binding = _GetBinding(record.sceneIndex);
    if(binding == nullptr)
    {
      _meshBindings[record.sceneIndex] = UploadMeshToRenderer(engine, record.data, materialRecords);
      _frameVisibilityDirty = true;
      continue;
    }

    ::MeshGeometry geometry;
    ConvertHydraMeshToGeometry(record.data, geometry);
    engine.updateMeshGeometry(static_cast<uint32_t>(binding->rendererMeshIndex), geometry);
  }
}

bool EngineSceneSync::_UpdateInstances(::Engine& engine, SceneStore& sceneStore)
{
  bool updatedAnyInstance = false;
  const std::vector<MaterialRecord> materialRecords = sceneStore.GetMaterialRecordsSnapshot();

  for(const MeshRecord& record : sceneStore.ConsumeDirtyMeshInstances())
  {
    updatedAnyInstance = true;
    RendererMeshBinding* binding = _GetBinding(record.sceneIndex);
    if(binding == nullptr && record.active && record.data.valid)
    {
      _meshBindings[record.sceneIndex] = UploadMeshToRenderer(engine, record.data, materialRecords);
      binding = _GetBinding(record.sceneIndex);
    }
    if(binding == nullptr)
    {
      continue;
    }

    EnsureRendererInstanceSlots(engine, *binding, record.data);
    UpdateRendererInstances(engine, record, binding->rendererInstanceIndices, TfTokenVector{});
  }
  return updatedAnyInstance;
}

void EngineSceneSync::_UpdateMaterials(::Engine& engine, SceneStore& sceneStore)
{
  const std::vector<MaterialRecord> materialRecords = sceneStore.GetMaterialRecordsSnapshot();
  const std::vector<MeshRecord> meshRecords = sceneStore.GetMeshRecordsSnapshot();
  const std::vector<uint32_t> dirtyMaterialIndices = sceneStore.ConsumeDirtyMaterialIndices();
  const std::set<uint32_t> dirtyMaterialSet(dirtyMaterialIndices.begin(), dirtyMaterialIndices.end());
  std::vector<::MaterialUpdate> newMaterials;
  for(const MeshRecord& record : meshRecords)
  {
    const RendererMeshBinding* binding = _GetBinding(record.sceneIndex);
    if(binding == nullptr)
    {
      continue;
    }
    const HydraMesh& curMesh = record.data;
    for(size_t localMatIdx = 0; localMatIdx < curMesh.scene_mat_ids.size(); ++localMatIdx)
    {
      int globalMatId = curMesh.scene_mat_ids[localMatIdx];
      if(globalMatId < 0 || static_cast<size_t>(globalMatId) >= materialRecords.size())
      {
        globalMatId = 0;
      }
      if(dirtyMaterialSet.find(static_cast<uint32_t>(globalMatId)) != dirtyMaterialSet.end())
      {
        ::Material newMaterial = ToMaterial(materialRecords[static_cast<size_t>(globalMatId)].data);
        newMaterials.push_back({binding->rendererMeshIndex, static_cast<int>(localMatIdx), newMaterial});
      }
    }
  }
  if(!newMaterials.empty())
  {
    engine.updateMaterialsAtRuntime(newMaterials);
  }
}

} // namespace hdrobot

PXR_NAMESPACE_CLOSE_SCOPE
