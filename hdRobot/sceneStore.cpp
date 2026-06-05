#include "sceneStore.h"

#include <algorithm>
#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

namespace hdrobot {

namespace
{
template <typename UpdateT>
void EnqueuePendingUpdate(std::mutex& mutex, std::vector<UpdateT>& pendingUpdates, UpdateT update)
{
  if(!IsValid(update.handle))
  {
    return;
  }
  std::lock_guard guard(mutex);
  pendingUpdates.push_back(std::move(update));
}

template <typename SlotVectorT, typename UpdateT>
void ApplySceneDataUpdates(SlotVectorT& records, std::vector<UpdateT>& updates)
{
  for(UpdateT& update : updates)
  {
    auto* record = records.Get(update.handle);
    if(record == nullptr)
    {
      continue;
    }
    record->data = std::move(update.data);
    record->active = update.active;
    record->dirty = record->dirty || update.dirty;
  }
}

template <typename DataT, typename SlotVectorT, typename IncludeRecordFn>
std::vector<DataT> GetActiveDataSnapshot(const SlotVectorT& records, IncludeRecordFn includeRecord)
{
  std::vector<DataT> result;
  for(const auto& record : records.GetSnapshot())
  {
    if(includeRecord(record))
    {
      result.push_back(record.data);
    }
  }
  return result;
}
} // namespace

SceneStore::SceneStore()
{
  MaterialRecord defaultMaterial;
  defaultMaterial.id = SdfPath::EmptyPath();
  defaultMaterial.data.set_default();
  defaultMaterial.active = true;
  defaultMaterial.dirty = true;
  _defaultMaterialHandle = _materials.Allocate(std::move(defaultMaterial));
}

MeshHandle SceneStore::CreateMesh(const SdfPath& id)
{
  MeshRecord record;
  record.id = id;
  record.data.scene_mat_ids = {ResolveMaterialIndexForUpload(_defaultMaterialHandle)};

  std::lock_guard guard(_sceneMutex);
  MeshHandle handle = _meshes.Allocate(std::move(record));
  if(MeshRecord* storedRecord = _meshes.Get(handle))
  {
    storedRecord->sceneIndex = handle.index;
  }
  return handle;
}

MaterialHandle SceneStore::CreateMaterial(const SdfPath& id)
{
  MaterialRecord record;
  record.id = id;
  record.data.set_default();

  std::lock_guard guard(_sceneMutex);
  return _materials.Allocate(std::move(record));
}

LightHandle SceneStore::CreateLight(const SdfPath& id)
{
  LightRecord record;
  record.id = id;

  std::lock_guard guard(_sceneMutex);
  return _lights.Allocate(std::move(record));
}

CameraHandle SceneStore::CreateCamera(const SdfPath& id)
{
  CameraRecord record;
  record.id = id;
  record.data.name = id.GetString();

  std::lock_guard guard(_sceneMutex);
  return _cameras.Allocate(std::move(record));
}

LidarSensorHandle SceneStore::CreateLidarSensor(const SdfPath& id)
{
  LidarSensorRecord record;
  record.id = id;
  record.data.name = id.GetString();

  std::lock_guard guard(_sceneMutex);
  return _lidarSensors.Allocate(std::move(record));
}

HeightScanSensorHandle SceneStore::CreateHeightScanSensor(const SdfPath& id)
{
  HeightScanSensorRecord record;
  record.id = id;
  record.data.name = id.GetString();

  std::lock_guard guard(_sceneMutex);
  return _heightScanSensors.Allocate(std::move(record));
}

MaterialHandle SceneStore::GetDefaultMaterialHandle() const
{
  return _defaultMaterialHandle;
}

void SceneStore::DestroyMesh(MeshHandle handle)
{
  std::lock_guard guard(_sceneMutex);
  MeshRecord* record = _meshes.Get(handle);
  if(record == nullptr)
  {
    return;
  }
  record->active = false;
  record->data.valid = false;
  record->instanceDirty = true;
  _meshes.Retire(handle);
}

void SceneStore::DestroyMaterial(MaterialHandle handle)
{
  std::lock_guard guard(_sceneMutex);
  MaterialRecord* record = _materials.Get(handle);
  if(record == nullptr || handle.index == _defaultMaterialHandle.index)
  {
    return;
  }
  record->active = false;
  record->data.set_default();
  record->dirty = true;
  _materials.Retire(handle);
}

void SceneStore::DestroyLight(LightHandle handle)
{
  std::lock_guard guard(_sceneMutex);
  LightRecord* record = _lights.Get(handle);
  if(record == nullptr)
  {
    return;
  }
  record->active = false;
  record->data.valid = 0;
  record->dirty = true;
  _lights.Retire(handle);
}

void SceneStore::DestroyCamera(CameraHandle handle)
{
  std::lock_guard guard(_sceneMutex);
  CameraRecord* record = _cameras.Get(handle);
  if(record == nullptr)
  {
    return;
  }
  record->active = false;
  record->dirty = true;
  _cameras.Retire(handle);
}

void SceneStore::DestroyLidarSensor(LidarSensorHandle handle)
{
  std::lock_guard guard(_sceneMutex);
  LidarSensorRecord* record = _lidarSensors.Get(handle);
  if(record == nullptr)
  {
    return;
  }
  record->active = false;
  record->dirty = true;
  _lidarSensors.Retire(handle);
}

void SceneStore::DestroyHeightScanSensor(HeightScanSensorHandle handle)
{
  std::lock_guard guard(_sceneMutex);
  HeightScanSensorRecord* record = _heightScanSensors.Get(handle);
  if(record == nullptr)
  {
    return;
  }
  record->active = false;
  record->dirty = true;
  _heightScanSensors.Retire(handle);
}

void SceneStore::EnqueueMeshUpdate(MeshUpdate update)
{
  EnqueuePendingUpdate(_pendingMutex, _pendingMeshUpdates, std::move(update));
}

void SceneStore::EnqueueMaterialUpdate(MaterialUpdate update)
{
  EnqueuePendingUpdate(_pendingMutex, _pendingMaterialUpdates, std::move(update));
}

void SceneStore::EnqueueLightUpdate(LightUpdate update)
{
  EnqueuePendingUpdate(_pendingMutex, _pendingLightUpdates, std::move(update));
}

void SceneStore::EnqueueCameraUpdate(CameraUpdate update)
{
  EnqueuePendingUpdate(_pendingMutex, _pendingCameraUpdates, std::move(update));
}

void SceneStore::EnqueueLidarSensorUpdate(LidarSensorUpdate update)
{
  EnqueuePendingUpdate(_pendingMutex, _pendingLidarSensorUpdates, std::move(update));
}

void SceneStore::EnqueueHeightScanSensorUpdate(HeightScanSensorUpdate update)
{
  EnqueuePendingUpdate(_pendingMutex, _pendingHeightScanSensorUpdates, std::move(update));
}

void SceneStore::ApplyPendingUpdates()
{
  std::vector<MeshUpdate> meshUpdates;
  std::vector<MaterialUpdate> materialUpdates;
  std::vector<LightUpdate> lightUpdates;
  std::vector<CameraUpdate> cameraUpdates;
  std::vector<LidarSensorUpdate> lidarSensorUpdates;
  std::vector<HeightScanSensorUpdate> heightScanSensorUpdates;
  {
    std::lock_guard guard(_pendingMutex);
    meshUpdates.swap(_pendingMeshUpdates);
    materialUpdates.swap(_pendingMaterialUpdates);
    lightUpdates.swap(_pendingLightUpdates);
    cameraUpdates.swap(_pendingCameraUpdates);
    lidarSensorUpdates.swap(_pendingLidarSensorUpdates);
    heightScanSensorUpdates.swap(_pendingHeightScanSensorUpdates);
  }

  std::lock_guard guard(_sceneMutex);
  ApplySceneDataUpdates(_materials, materialUpdates);
  ApplySceneDataUpdates(_lights, lightUpdates);
  ApplySceneDataUpdates(_cameras, cameraUpdates);
  ApplySceneDataUpdates(_lidarSensors, lidarSensorUpdates);
  ApplySceneDataUpdates(_heightScanSensors, heightScanSensorUpdates);
  _ApplyMeshUpdates(meshUpdates);
}

int SceneStore::RegisterTexturePath(const std::string& texturePath, TextureUsage usage)
{
  std::lock_guard guard(_textureRegistryMutex);
  return _textureRegistry.Register(texturePath, usage);
}

std::vector<TextureAsset> SceneStore::GetTextureAssetsSnapshot() const
{
  std::lock_guard guard(_textureRegistryMutex);
  return _textureRegistry.GetTextureAssets();
}

uint64_t SceneStore::GetTextureRegistryVersion() const
{
  std::lock_guard guard(_textureRegistryMutex);
  return _textureRegistry.GetVersion();
}

std::optional<HydraMaterial> SceneStore::GetMaterialDataCopy(MaterialHandle handle) const
{
  std::lock_guard guard(_sceneMutex);
  const MaterialRecord* record = _materials.Get(handle);
  if(record == nullptr)
  {
    return std::nullopt;
  }
  return record->data;
}

int SceneStore::ResolveMaterialIndexForUpload(MaterialHandle handle) const
{
  if(!IsValid(handle))
  {
    return static_cast<int>(_defaultMaterialHandle.index);
  }
  std::lock_guard guard(_sceneMutex);
  return _materials.Get(handle) == nullptr ? static_cast<int>(_defaultMaterialHandle.index)
                                           : static_cast<int>(handle.index);
}

std::vector<MeshRecord> SceneStore::GetMeshRecordsSnapshot() const
{
  std::lock_guard guard(_sceneMutex);
  return _meshes.GetSnapshot();
}

std::vector<MaterialRecord> SceneStore::GetMaterialRecordsSnapshot() const
{
  std::lock_guard guard(_sceneMutex);
  return _materials.GetSnapshot();
}

std::vector<HydraLight> SceneStore::GetActiveLightSnapshot() const
{
  std::lock_guard guard(_sceneMutex);
  return GetActiveDataSnapshot<HydraLight>(_lights, [](const LightRecord& record) {
    return record.active && record.data.valid;
  });
}

std::vector<CameraData> SceneStore::GetActiveCameraSnapshot() const
{
  std::lock_guard guard(_sceneMutex);
  return GetActiveDataSnapshot<CameraData>(_cameras,
                                                  [](const CameraRecord& record) { return record.active; });
}

std::vector<LidarSensorData> SceneStore::GetActiveLidarSensorSnapshot() const
{
  std::lock_guard guard(_sceneMutex);
  return GetActiveDataSnapshot<LidarSensorData>(
      _lidarSensors, [](const LidarSensorRecord& record) { return record.active; });
}

std::vector<HeightScanSensorData> SceneStore::GetActiveHeightScanSensorSnapshot() const
{
  std::lock_guard guard(_sceneMutex);
  return GetActiveDataSnapshot<HeightScanSensorData>(
      _heightScanSensors, [](const HeightScanSensorRecord& record) { return record.active; });
}

std::vector<MeshRecord> SceneStore::ConsumeDirtyMeshGeometry()
{
  std::vector<MeshRecord> result;
  std::lock_guard guard(_sceneMutex);
  for(uint32_t index = 0; index < _meshes.Size(); ++index)
  {
    MeshRecord* record = _meshes.GetByIndex(index);
    if(record == nullptr || !record->geometryDirty)
    {
      continue;
    }
    result.push_back(*record);
    record->geometryDirty = false;
  }
  return result;
}

std::vector<MeshRecord> SceneStore::ConsumeDirtyMeshInstances()
{
  std::vector<MeshRecord> result;
  std::lock_guard guard(_sceneMutex);
  for(uint32_t index = 0; index < _meshes.Size(); ++index)
  {
    MeshRecord* record = _meshes.GetByIndex(index);
    if(record == nullptr || !record->instanceDirty)
    {
      continue;
    }
    result.push_back(*record);
    record->instanceDirty = false;
  }
  return result;
}

std::vector<uint32_t> SceneStore::ConsumeDirtyMaterialIndices()
{
  std::vector<uint32_t> result;
  std::lock_guard guard(_sceneMutex);
  for(uint32_t index = 0; index < _materials.Size(); ++index)
  {
    MaterialRecord* record = _materials.GetByIndex(index);
    if(record == nullptr || !record->dirty)
    {
      continue;
    }
    result.push_back(index);
    record->dirty = false;
  }
  return result;
}

void SceneStore::_ApplyMeshUpdates(std::vector<MeshUpdate>& updates)
{
  for(MeshUpdate& update : updates)
  {
    MeshRecord* record = _meshes.Get(update.handle);
    if(record == nullptr)
    {
      continue;
    }
    if(update.data.has_value())
    {
      record->data = std::move(*update.data);
    }
    if(update.active.has_value())
    {
      record->active = *update.active;
      record->data.valid = *update.active;
    }
    record->geometryDirty = record->geometryDirty || update.geometryDirty;
    record->instanceDirty = record->instanceDirty || update.instanceDirty;
  }
}

} // namespace hdrobot

PXR_NAMESPACE_CLOSE_SCOPE
