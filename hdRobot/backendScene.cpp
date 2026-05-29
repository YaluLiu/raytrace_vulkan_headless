#include "backendScene.h"

#include <algorithm>
#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

HdRobotBackendScene::HdRobotBackendScene()
{
  HdRobotMaterialRecord defaultMaterial;
  defaultMaterial.id = SdfPath::EmptyPath();
  defaultMaterial.data.set_default();
  defaultMaterial.active = true;
  defaultMaterial.dirty = true;
  _defaultMaterialHandle = _materials.Allocate(std::move(defaultMaterial));
}

HdRobotMeshHandle HdRobotBackendScene::CreateMesh(const SdfPath& id)
{
  HdRobotMeshRecord record;
  record.id = id;
  record.data.scene_mat_ids = {ResolveMaterialIndexForUpload(_defaultMaterialHandle)};

  std::lock_guard guard(_sceneMutex);
  HdRobotMeshHandle handle = _meshes.Allocate(std::move(record));
  if(HdRobotMeshRecord* storedRecord = _meshes.Get(handle))
  {
    storedRecord->backendIndex = handle.index;
  }
  return handle;
}

HdRobotMaterialHandle HdRobotBackendScene::CreateMaterial(const SdfPath& id)
{
  HdRobotMaterialRecord record;
  record.id = id;
  record.data.set_default();

  std::lock_guard guard(_sceneMutex);
  return _materials.Allocate(std::move(record));
}

HdRobotLightHandle HdRobotBackendScene::CreateLight(const SdfPath& id)
{
  HdRobotLightRecord record;
  record.id = id;

  std::lock_guard guard(_sceneMutex);
  return _lights.Allocate(std::move(record));
}

HdRobotCameraHandle HdRobotBackendScene::CreateCamera(const SdfPath& id)
{
  HdRobotCameraRecord record;
  record.id = id;
  record.data.name = id.GetString();

  std::lock_guard guard(_sceneMutex);
  return _cameras.Allocate(std::move(record));
}

HdRobotLidarSensorHandle HdRobotBackendScene::CreateLidarSensor(const SdfPath& id)
{
  HdRobotLidarSensorRecord record;
  record.id = id;
  record.data.name = id.GetString();

  std::lock_guard guard(_sceneMutex);
  return _lidarSensors.Allocate(std::move(record));
}

HdRobotHeightScanSensorHandle HdRobotBackendScene::CreateHeightScanSensor(const SdfPath& id)
{
  HdRobotHeightScanSensorRecord record;
  record.id = id;
  record.data.name = id.GetString();

  std::lock_guard guard(_sceneMutex);
  return _heightScanSensors.Allocate(std::move(record));
}

HdRobotMaterialHandle HdRobotBackendScene::GetDefaultMaterialHandle() const
{
  return _defaultMaterialHandle;
}

void HdRobotBackendScene::DestroyMesh(HdRobotMeshHandle handle)
{
  std::lock_guard guard(_sceneMutex);
  HdRobotMeshRecord* record = _meshes.Get(handle);
  if(record == nullptr)
  {
    return;
  }
  record->active = false;
  record->data.valid = false;
  record->instanceDirty = true;
  _meshes.Retire(handle);
}

void HdRobotBackendScene::DestroyMaterial(HdRobotMaterialHandle handle)
{
  std::lock_guard guard(_sceneMutex);
  HdRobotMaterialRecord* record = _materials.Get(handle);
  if(record == nullptr || handle.index == _defaultMaterialHandle.index)
  {
    return;
  }
  record->active = false;
  record->data.set_default();
  record->dirty = true;
  _materials.Retire(handle);
}

void HdRobotBackendScene::DestroyLight(HdRobotLightHandle handle)
{
  std::lock_guard guard(_sceneMutex);
  HdRobotLightRecord* record = _lights.Get(handle);
  if(record == nullptr)
  {
    return;
  }
  record->active = false;
  record->data.valid = 0;
  record->dirty = true;
  _lights.Retire(handle);
}

void HdRobotBackendScene::DestroyCamera(HdRobotCameraHandle handle)
{
  std::lock_guard guard(_sceneMutex);
  HdRobotCameraRecord* record = _cameras.Get(handle);
  if(record == nullptr)
  {
    return;
  }
  record->active = false;
  record->dirty = true;
  _cameras.Retire(handle);
}

void HdRobotBackendScene::DestroyLidarSensor(HdRobotLidarSensorHandle handle)
{
  std::lock_guard guard(_sceneMutex);
  HdRobotLidarSensorRecord* record = _lidarSensors.Get(handle);
  if(record == nullptr)
  {
    return;
  }
  record->active = false;
  record->dirty = true;
  _lidarSensors.Retire(handle);
}

void HdRobotBackendScene::DestroyHeightScanSensor(HdRobotHeightScanSensorHandle handle)
{
  std::lock_guard guard(_sceneMutex);
  HdRobotHeightScanSensorRecord* record = _heightScanSensors.Get(handle);
  if(record == nullptr)
  {
    return;
  }
  record->active = false;
  record->dirty = true;
  _heightScanSensors.Retire(handle);
}

void HdRobotBackendScene::EnqueueMeshUpdate(HdRobotMeshUpdate update)
{
  if(!IsValid(update.handle))
  {
    return;
  }
  std::lock_guard guard(_pendingMutex);
  _pendingMeshUpdates.push_back(std::move(update));
}

void HdRobotBackendScene::EnqueueMaterialUpdate(HdRobotMaterialUpdate update)
{
  if(!IsValid(update.handle))
  {
    return;
  }
  std::lock_guard guard(_pendingMutex);
  _pendingMaterialUpdates.push_back(std::move(update));
}

void HdRobotBackendScene::EnqueueLightUpdate(HdRobotLightUpdate update)
{
  if(!IsValid(update.handle))
  {
    return;
  }
  std::lock_guard guard(_pendingMutex);
  _pendingLightUpdates.push_back(std::move(update));
}

void HdRobotBackendScene::EnqueueCameraUpdate(HdRobotCameraUpdate update)
{
  if(!IsValid(update.handle))
  {
    return;
  }
  std::lock_guard guard(_pendingMutex);
  _pendingCameraUpdates.push_back(std::move(update));
}

void HdRobotBackendScene::EnqueueLidarSensorUpdate(HdRobotLidarSensorUpdate update)
{
  if(!IsValid(update.handle))
  {
    return;
  }
  std::lock_guard guard(_pendingMutex);
  _pendingLidarSensorUpdates.push_back(std::move(update));
}

void HdRobotBackendScene::EnqueueHeightScanSensorUpdate(HdRobotHeightScanSensorUpdate update)
{
  if(!IsValid(update.handle))
  {
    return;
  }
  std::lock_guard guard(_pendingMutex);
  _pendingHeightScanSensorUpdates.push_back(std::move(update));
}

void HdRobotBackendScene::ApplyPendingUpdates()
{
  std::vector<HdRobotMeshUpdate> meshUpdates;
  std::vector<HdRobotMaterialUpdate> materialUpdates;
  std::vector<HdRobotLightUpdate> lightUpdates;
  std::vector<HdRobotCameraUpdate> cameraUpdates;
  std::vector<HdRobotLidarSensorUpdate> lidarSensorUpdates;
  std::vector<HdRobotHeightScanSensorUpdate> heightScanSensorUpdates;
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
  _ApplyMaterialUpdates(materialUpdates);
  _ApplyLightUpdates(lightUpdates);
  _ApplyCameraUpdates(cameraUpdates);
  _ApplyLidarSensorUpdates(lidarSensorUpdates);
  _ApplyHeightScanSensorUpdates(heightScanSensorUpdates);
  _ApplyMeshUpdates(meshUpdates);
}

std::optional<HydraMaterial> HdRobotBackendScene::GetMaterialDataCopy(HdRobotMaterialHandle handle) const
{
  std::lock_guard guard(_sceneMutex);
  const HdRobotMaterialRecord* record = _materials.Get(handle);
  if(record == nullptr)
  {
    return std::nullopt;
  }
  return record->data;
}

int HdRobotBackendScene::ResolveMaterialIndexForUpload(HdRobotMaterialHandle handle) const
{
  if(!IsValid(handle))
  {
    return static_cast<int>(_defaultMaterialHandle.index);
  }
  std::lock_guard guard(_sceneMutex);
  return _materials.Get(handle) == nullptr ? static_cast<int>(_defaultMaterialHandle.index)
                                           : static_cast<int>(handle.index);
}

std::vector<HdRobotMeshRecord> HdRobotBackendScene::GetMeshRecordsSnapshot() const
{
  std::lock_guard guard(_sceneMutex);
  return _meshes.GetSnapshot();
}

std::vector<HdRobotMaterialRecord> HdRobotBackendScene::GetMaterialRecordsSnapshot() const
{
  std::lock_guard guard(_sceneMutex);
  return _materials.GetSnapshot();
}

std::vector<HydraLight> HdRobotBackendScene::GetActiveLightSnapshot() const
{
  std::vector<HydraLight> result;
  std::lock_guard guard(_sceneMutex);
  for(const HdRobotLightRecord& record : _lights.GetSnapshot())
  {
    if(record.active && record.data.valid)
    {
      result.push_back(record.data);
    }
  }
  return result;
}

std::vector<HdRobotCameraData> HdRobotBackendScene::GetActiveCameraSnapshot() const
{
  std::vector<HdRobotCameraData> result;
  std::lock_guard guard(_sceneMutex);
  for(const HdRobotCameraRecord& record : _cameras.GetSnapshot())
  {
    if(record.active)
    {
      result.push_back(record.data);
    }
  }
  return result;
}

std::vector<HdRobotLidarSensorData> HdRobotBackendScene::GetActiveLidarSensorSnapshot() const
{
  std::vector<HdRobotLidarSensorData> result;
  std::lock_guard guard(_sceneMutex);
  for(const HdRobotLidarSensorRecord& record : _lidarSensors.GetSnapshot())
  {
    if(record.active)
    {
      result.push_back(record.data);
    }
  }
  return result;
}

std::vector<HdRobotHeightScanSensorData> HdRobotBackendScene::GetActiveHeightScanSensorSnapshot() const
{
  std::vector<HdRobotHeightScanSensorData> result;
  std::lock_guard guard(_sceneMutex);
  for(const HdRobotHeightScanSensorRecord& record : _heightScanSensors.GetSnapshot())
  {
    if(record.active)
    {
      result.push_back(record.data);
    }
  }
  return result;
}

std::vector<HdRobotMeshRecord> HdRobotBackendScene::ConsumeDirtyMeshGeometry()
{
  std::vector<HdRobotMeshRecord> result;
  std::lock_guard guard(_sceneMutex);
  for(uint32_t index = 0; index < _meshes.Size(); ++index)
  {
    HdRobotMeshRecord* record = _meshes.GetByIndex(index);
    if(record == nullptr || !record->geometryDirty)
    {
      continue;
    }
    result.push_back(*record);
    record->geometryDirty = false;
  }
  return result;
}

std::vector<HdRobotMeshRecord> HdRobotBackendScene::ConsumeDirtyMeshInstances()
{
  std::vector<HdRobotMeshRecord> result;
  std::lock_guard guard(_sceneMutex);
  for(uint32_t index = 0; index < _meshes.Size(); ++index)
  {
    HdRobotMeshRecord* record = _meshes.GetByIndex(index);
    if(record == nullptr || !record->instanceDirty)
    {
      continue;
    }
    result.push_back(*record);
    record->instanceDirty = false;
  }
  return result;
}

std::vector<uint32_t> HdRobotBackendScene::ConsumeDirtyMaterialIndices()
{
  std::vector<uint32_t> result;
  std::lock_guard guard(_sceneMutex);
  for(uint32_t index = 0; index < _materials.Size(); ++index)
  {
    HdRobotMaterialRecord* record = _materials.GetByIndex(index);
    if(record == nullptr || !record->dirty)
    {
      continue;
    }
    result.push_back(index);
    record->dirty = false;
  }
  return result;
}

void HdRobotBackendScene::MarkAllMeshesInstanceDirty()
{
  std::lock_guard guard(_sceneMutex);
  for(uint32_t index = 0; index < _meshes.Size(); ++index)
  {
    HdRobotMeshRecord* record = _meshes.GetByIndex(index);
    if(record != nullptr)
    {
      record->instanceDirty = true;
    }
  }
}

void HdRobotBackendScene::SetMeshRendererState(uint32_t meshIndex,
                                               int rendererMeshId,
                                               std::vector<int> rendererInstanceIds)
{
  std::lock_guard guard(_sceneMutex);
  HdRobotMeshRecord* record = _meshes.GetByIndex(meshIndex);
  if(record == nullptr)
  {
    return;
  }
  record->rendererMeshId = rendererMeshId;
  record->rendererInstanceIds = std::move(rendererInstanceIds);
  record->data.rendererInstanceIds = record->rendererInstanceIds;
  record->instanceDirty = true;
}

void HdRobotBackendScene::_ApplyMeshUpdates(std::vector<HdRobotMeshUpdate>& updates)
{
  for(HdRobotMeshUpdate& update : updates)
  {
    HdRobotMeshRecord* record = _meshes.Get(update.handle);
    if(record == nullptr)
    {
      continue;
    }
    if(update.data.has_value())
    {
      std::vector<int> rendererInstanceIds = std::move(record->rendererInstanceIds);
      const int rendererMeshId = record->rendererMeshId;
      record->data = std::move(*update.data);
      record->rendererMeshId = rendererMeshId;
      record->rendererInstanceIds = std::move(rendererInstanceIds);
      record->data.rendererInstanceIds = record->rendererInstanceIds;
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

void HdRobotBackendScene::_ApplyMaterialUpdates(std::vector<HdRobotMaterialUpdate>& updates)
{
  for(HdRobotMaterialUpdate& update : updates)
  {
    HdRobotMaterialRecord* record = _materials.Get(update.handle);
    if(record == nullptr)
    {
      continue;
    }
    record->data = std::move(update.data);
    record->active = update.active;
    record->dirty = record->dirty || update.dirty;
  }
}

void HdRobotBackendScene::_ApplyLightUpdates(std::vector<HdRobotLightUpdate>& updates)
{
  for(HdRobotLightUpdate& update : updates)
  {
    HdRobotLightRecord* record = _lights.Get(update.handle);
    if(record == nullptr)
    {
      continue;
    }
    record->data = std::move(update.data);
    record->active = update.active;
    record->dirty = record->dirty || update.dirty;
  }
}

void HdRobotBackendScene::_ApplyCameraUpdates(std::vector<HdRobotCameraUpdate>& updates)
{
  for(HdRobotCameraUpdate& update : updates)
  {
    HdRobotCameraRecord* record = _cameras.Get(update.handle);
    if(record == nullptr)
    {
      continue;
    }
    record->data = std::move(update.data);
    record->active = update.active;
    record->dirty = record->dirty || update.dirty;
  }
}

void HdRobotBackendScene::_ApplyLidarSensorUpdates(std::vector<HdRobotLidarSensorUpdate>& updates)
{
  for(HdRobotLidarSensorUpdate& update : updates)
  {
    HdRobotLidarSensorRecord* record = _lidarSensors.Get(update.handle);
    if(record == nullptr)
    {
      continue;
    }
    record->data = std::move(update.data);
    record->active = update.active;
    record->dirty = record->dirty || update.dirty;
  }
}

void HdRobotBackendScene::_ApplyHeightScanSensorUpdates(std::vector<HdRobotHeightScanSensorUpdate>& updates)
{
  for(HdRobotHeightScanSensorUpdate& update : updates)
  {
    HdRobotHeightScanSensorRecord* record = _heightScanSensors.Get(update.handle);
    if(record == nullptr)
    {
      continue;
    }
    record->data = std::move(update.data);
    record->active = update.active;
    record->dirty = record->dirty || update.dirty;
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
