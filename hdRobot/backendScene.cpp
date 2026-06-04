#include "backendScene.h"

#include <algorithm>
#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

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
void ApplyBackendDataUpdates(SlotVectorT& records, std::vector<UpdateT>& updates)
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
  EnqueuePendingUpdate(_pendingMutex, _pendingMeshUpdates, std::move(update));
}

void HdRobotBackendScene::EnqueueMaterialUpdate(HdRobotMaterialUpdate update)
{
  EnqueuePendingUpdate(_pendingMutex, _pendingMaterialUpdates, std::move(update));
}

void HdRobotBackendScene::EnqueueLightUpdate(HdRobotLightUpdate update)
{
  EnqueuePendingUpdate(_pendingMutex, _pendingLightUpdates, std::move(update));
}

void HdRobotBackendScene::EnqueueCameraUpdate(HdRobotCameraUpdate update)
{
  EnqueuePendingUpdate(_pendingMutex, _pendingCameraUpdates, std::move(update));
}

void HdRobotBackendScene::EnqueueLidarSensorUpdate(HdRobotLidarSensorUpdate update)
{
  EnqueuePendingUpdate(_pendingMutex, _pendingLidarSensorUpdates, std::move(update));
}

void HdRobotBackendScene::EnqueueHeightScanSensorUpdate(HdRobotHeightScanSensorUpdate update)
{
  EnqueuePendingUpdate(_pendingMutex, _pendingHeightScanSensorUpdates, std::move(update));
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
  ApplyBackendDataUpdates(_materials, materialUpdates);
  ApplyBackendDataUpdates(_lights, lightUpdates);
  ApplyBackendDataUpdates(_cameras, cameraUpdates);
  ApplyBackendDataUpdates(_lidarSensors, lidarSensorUpdates);
  ApplyBackendDataUpdates(_heightScanSensors, heightScanSensorUpdates);
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
  std::lock_guard guard(_sceneMutex);
  return GetActiveDataSnapshot<HydraLight>(_lights, [](const HdRobotLightRecord& record) {
    return record.active && record.data.valid;
  });
}

std::vector<HdRobotCameraData> HdRobotBackendScene::GetActiveCameraSnapshot() const
{
  std::lock_guard guard(_sceneMutex);
  return GetActiveDataSnapshot<HdRobotCameraData>(_cameras,
                                                  [](const HdRobotCameraRecord& record) { return record.active; });
}

std::vector<HdRobotLidarSensorData> HdRobotBackendScene::GetActiveLidarSensorSnapshot() const
{
  std::lock_guard guard(_sceneMutex);
  return GetActiveDataSnapshot<HdRobotLidarSensorData>(
      _lidarSensors, [](const HdRobotLidarSensorRecord& record) { return record.active; });
}

std::vector<HdRobotHeightScanSensorData> HdRobotBackendScene::GetActiveHeightScanSensorSnapshot() const
{
  std::lock_guard guard(_sceneMutex);
  return GetActiveDataSnapshot<HdRobotHeightScanSensorData>(
      _heightScanSensors, [](const HdRobotHeightScanSensorRecord& record) { return record.active; });
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

PXR_NAMESPACE_CLOSE_SCOPE
