#include "engineSceneSync.h"

#include "glInteropCache.h"
#include "hydraTextureAssetExport.h"
#include "renderBridgeConversions.h"
#include "tokens.h"

#include <engine/engine.hpp>

#include <algorithm>
#include <set>
#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

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

std::vector<Material> CollectMaterialsForMesh(const HydraMesh& mesh,
                                              const std::vector<HdRobotMaterialRecord>& materials)
{
  std::vector<Material> materialsForUpload;
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

HdRobotEngineSceneSync::RendererMeshBinding UploadMeshToRenderer(
    ::Engine& engine,
    const HydraMesh& mesh,
    const std::vector<HdRobotMaterialRecord>& materials)
{
  HdRobotEngineSceneSync::RendererMeshBinding binding;
  binding.rendererMeshId = static_cast<int>(engine.getMeshSourceCount());

  MeshGeometry geometry;
  ConvertHydraMeshToGeometry(mesh, geometry);
  std::vector<Material> materialsForUpload = CollectMaterialsForMesh(mesh, materials);
  engine.uploadMesh(geometry, materialsForUpload);

  const size_t firstInstanceId = engine.getInstanceCount() - 1;
  const auto instance = engine.getInstance(firstInstanceId);
  const size_t authoredInstanceCount = mesh.instanceTransforms.size();
  const size_t rendererInstanceSlotCount = std::max<size_t>(authoredInstanceCount, 1);
  for(size_t i = 1; i < rendererInstanceSlotCount; ++i)
  {
    engine.addInstance(instance.transform, instance.objIndex, static_cast<int>(i));
  }
  binding.rendererInstanceIds.reserve(rendererInstanceSlotCount);
  for(size_t i = 0; i < rendererInstanceSlotCount; ++i)
  {
    binding.rendererInstanceIds.push_back(static_cast<int>(i + firstInstanceId));
  }
  return binding;
}

bool EnsureRendererInstanceSlots(::Engine& engine, HdRobotEngineSceneSync::RendererMeshBinding& binding, const HydraMesh& mesh)
{
  if(binding.rendererInstanceIds.empty())
  {
    return false;
  }

  const size_t requiredSlotCount = std::max<size_t>(mesh.instanceTransforms.size(), 1);
  if(binding.rendererInstanceIds.size() >= requiredSlotCount)
  {
    return false;
  }

  const InstanceInfo baseInstance = engine.getInstance(static_cast<size_t>(binding.rendererInstanceIds.front()));
  for(size_t i = binding.rendererInstanceIds.size(); i < requiredSlotCount; ++i)
  {
    const uint32_t rendererInstanceId = engine.addInstance(baseInstance.transform, baseInstance.objIndex, static_cast<int>(i));
    binding.rendererInstanceIds.push_back(static_cast<int>(rendererInstanceId));
  }
  return true;
}

void UpdateRendererInstances(::Engine& engine,
                             const HdRobotMeshRecord& record,
                             const std::vector<int>& rendererInstanceIds,
                             const TfTokenVector& renderTags)
{
  const HydraMesh& mesh = record.data;
  const bool tagMatched = IsMeshRenderTagMatched(mesh, renderTags);
  const bool meshVisible = record.active && mesh.visible && mesh.valid && tagMatched;
  const uint32_t traceMask = mesh.traceRole == HdRobotTraceRoleTokens->ground
                                 ? kTraceMaskGround
                                 : kTraceMaskDefaultGeometry;

  for(size_t instanceId = 0; instanceId < rendererInstanceIds.size(); ++instanceId)
  {
    const int rendererInstanceId = rendererInstanceIds[instanceId];
    if(rendererInstanceId < 0)
    {
      continue;
    }

    const bool instanceValid = mesh.hasInstances && (instanceId < mesh.instanceTransforms.size());
    const bool visible = meshVisible && instanceValid;
    glm::mat4 transform = glm::transpose(mesh.transform);
    if(instanceValid)
    {
      transform = glm::transpose(mesh.transform * mesh.instanceTransforms[instanceId]);
    }
    engine.updateInstance(static_cast<uint32_t>(rendererInstanceId), transform, visible, traceMask);
  }
}
} // namespace

HdRobotEngineSceneSync::RendererMeshBinding* HdRobotEngineSceneSync::_GetBinding(uint32_t sceneIndex)
{
  const auto bindingIt = _meshBindings.find(sceneIndex);
  return bindingIt == _meshBindings.end() ? nullptr : &bindingIt->second;
}

const HdRobotEngineSceneSync::RendererMeshBinding* HdRobotEngineSceneSync::_GetBinding(uint32_t sceneIndex) const
{
  const auto bindingIt = _meshBindings.find(sceneIndex);
  return bindingIt == _meshBindings.end() ? nullptr : &bindingIt->second;
}

void HdRobotEngineSceneSync::EnsureResources(::Engine& engine,
                                             HdRobotSceneStore& sceneStore,
                                             ::HdRobotGlInteropCache& glInteropCache)
{
  _EnsureInitialResources(engine, sceneStore);
  _RefreshTextureAssetsIfNeeded(engine, sceneStore, glInteropCache);
}

void HdRobotEngineSceneSync::SyncSceneToEngine(::Engine& engine, HdRobotSceneStore& sceneStore)
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

void HdRobotEngineSceneSync::ApplyFrameRenderTags(::Engine& engine, const TfTokenVector& renderTags)
{
  TfTokenVector normalizedRenderTags = NormalizeRenderTags(renderTags);
  const bool tagsChanged = !RenderTagsEqual(_activeRenderTags, normalizedRenderTags);
  if(!tagsChanged && !_frameVisibilityDirty)
  {
    return;
  }

  _activeRenderTags = std::move(normalizedRenderTags);
  for(const HdRobotMeshRecord& record : _frameMeshRecords)
  {
    const RendererMeshBinding* binding = _GetBinding(record.sceneIndex);
    if(binding == nullptr)
    {
      continue;
    }
    UpdateRendererInstances(engine, record, binding->rendererInstanceIds, _activeRenderTags);
  }
  _frameVisibilityDirty = false;
}

void HdRobotEngineSceneSync::_EnsureInitialResources(::Engine& engine, HdRobotSceneStore& sceneStore)
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

void HdRobotEngineSceneSync::_UploadInitialScene(::Engine& engine, HdRobotSceneStore& sceneStore)
{
  std::vector<HdRobotMeshRecord> meshRecords = sceneStore.GetMeshRecordsSnapshot();
  const std::vector<HdRobotMaterialRecord> materialRecords = sceneStore.GetMaterialRecordsSnapshot();
  for(const HdRobotMeshRecord& record : meshRecords)
  {
    if(!record.active || !record.data.valid)
    {
      continue;
    }
    _meshBindings[record.sceneIndex] = UploadMeshToRenderer(engine, record.data, materialRecords);
  }
}

void HdRobotEngineSceneSync::_RefreshTextureAssetsIfNeeded(::Engine& engine,
                                                           HdRobotSceneStore& sceneStore,
                                                           ::HdRobotGlInteropCache& glInteropCache)
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

void HdRobotEngineSceneSync::_UpdateLights(::Engine& engine, const HdRobotSceneStore& sceneStore)
{
  engine.clearLights();

  for(const HydraLight& curLight : sceneStore.GetActiveLightSnapshot())
  {
    engine.addLight(ToLight(curLight));
  }
}

void HdRobotEngineSceneSync::_UpdateGeometry(::Engine& engine, HdRobotSceneStore& sceneStore)
{
  const std::vector<HdRobotMaterialRecord> materialRecords = sceneStore.GetMaterialRecordsSnapshot();
  for(const HdRobotMeshRecord& record : sceneStore.ConsumeDirtyMeshGeometry())
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

    MeshGeometry geometry;
    ConvertHydraMeshToGeometry(record.data, geometry);
    engine.updateMeshGeometry(static_cast<uint32_t>(binding->rendererMeshId), geometry);
  }
}

bool HdRobotEngineSceneSync::_UpdateInstances(::Engine& engine, HdRobotSceneStore& sceneStore)
{
  bool updatedAnyInstance = false;
  const std::vector<HdRobotMaterialRecord> materialRecords = sceneStore.GetMaterialRecordsSnapshot();

  for(const HdRobotMeshRecord& record : sceneStore.ConsumeDirtyMeshInstances())
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
    UpdateRendererInstances(engine, record, binding->rendererInstanceIds, TfTokenVector{});
  }
  return updatedAnyInstance;
}

void HdRobotEngineSceneSync::_UpdateMaterials(::Engine& engine, HdRobotSceneStore& sceneStore)
{
  const std::vector<HdRobotMaterialRecord> materialRecords = sceneStore.GetMaterialRecordsSnapshot();
  const std::vector<HdRobotMeshRecord> meshRecords = sceneStore.GetMeshRecordsSnapshot();
  const std::vector<uint32_t> dirtyMaterialIndices = sceneStore.ConsumeDirtyMaterialIndices();
  const std::set<uint32_t> dirtyMaterialSet(dirtyMaterialIndices.begin(), dirtyMaterialIndices.end());
  std::vector<MaterialUpdate> newMaterials;
  for(const HdRobotMeshRecord& record : meshRecords)
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
        Material newMaterial = ToMaterial(materialRecords[static_cast<size_t>(globalMatId)].data);
        newMaterials.push_back({binding->rendererMeshId, static_cast<int>(localMatIdx), newMaterial});
      }
    }
  }
  if(!newMaterials.empty())
  {
    engine.updateMaterialsAtRuntime(newMaterials);
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
