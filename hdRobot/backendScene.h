#pragma once

#include "backendHandles.h"
#include "camera.h"
#include "heightScanSensor.h"
#include "lidarSensor.h"
#include "sceneData.h"
#include "slotVector.h"

#include <pxr/usd/sdf/path.h>

#include <mutex>
#include <optional>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

struct HdRobotMeshRecord
{
  uint32_t backendIndex = UINT32_MAX;
  SdfPath id;
  HydraMesh data;
  bool active = true;
  bool geometryDirty = true;
  bool instanceDirty = true;
  int rendererMeshId = -1;
  std::vector<int> rendererInstanceIds;
};

struct HdRobotMaterialRecord
{
  SdfPath id;
  HydraMaterial data;
  bool active = true;
  bool dirty = true;
};

struct HdRobotLightRecord
{
  SdfPath id;
  HydraLight data;
  bool active = true;
  bool dirty = true;
};

struct HdRobotCameraRecord
{
  SdfPath id;
  HdRobotCameraData data;
  bool active = true;
  bool dirty = true;
};

struct HdRobotLidarSensorRecord
{
  SdfPath id;
  HdRobotLidarSensorData data;
  bool active = true;
  bool dirty = true;
};

struct HdRobotHeightScanSensorRecord
{
  SdfPath id;
  HdRobotHeightScanSensorData data;
  bool active = true;
  bool dirty = true;
};

struct HdRobotMeshUpdate
{
  HdRobotMeshHandle handle;
  std::optional<HydraMesh> data;
  std::optional<bool> active;
  bool geometryDirty = false;
  bool instanceDirty = false;
};

struct HdRobotMaterialUpdate
{
  HdRobotMaterialHandle handle;
  HydraMaterial data;
  bool active = true;
  bool dirty = true;
};

struct HdRobotLightUpdate
{
  HdRobotLightHandle handle;
  HydraLight data;
  bool active = true;
  bool dirty = true;
};

struct HdRobotCameraUpdate
{
  HdRobotCameraHandle handle;
  HdRobotCameraData data;
  bool active = true;
  bool dirty = true;
};

struct HdRobotLidarSensorUpdate
{
  HdRobotLidarSensorHandle handle;
  HdRobotLidarSensorData data;
  bool active = true;
  bool dirty = true;
};

struct HdRobotHeightScanSensorUpdate
{
  HdRobotHeightScanSensorHandle handle;
  HdRobotHeightScanSensorData data;
  bool active = true;
  bool dirty = true;
};

class HdRobotBackendScene
{
public:
  HdRobotBackendScene();

  HdRobotMeshHandle CreateMesh(const SdfPath& id);
  HdRobotMaterialHandle CreateMaterial(const SdfPath& id);
  HdRobotLightHandle CreateLight(const SdfPath& id);
  HdRobotCameraHandle CreateCamera(const SdfPath& id);
  HdRobotLidarSensorHandle CreateLidarSensor(const SdfPath& id);
  HdRobotHeightScanSensorHandle CreateHeightScanSensor(const SdfPath& id);

  HdRobotMaterialHandle GetDefaultMaterialHandle() const;

  void DestroyMesh(HdRobotMeshHandle handle);
  void DestroyMaterial(HdRobotMaterialHandle handle);
  void DestroyLight(HdRobotLightHandle handle);
  void DestroyCamera(HdRobotCameraHandle handle);
  void DestroyLidarSensor(HdRobotLidarSensorHandle handle);
  void DestroyHeightScanSensor(HdRobotHeightScanSensorHandle handle);

  void EnqueueMeshUpdate(HdRobotMeshUpdate update);
  void EnqueueMaterialUpdate(HdRobotMaterialUpdate update);
  void EnqueueLightUpdate(HdRobotLightUpdate update);
  void EnqueueCameraUpdate(HdRobotCameraUpdate update);
  void EnqueueLidarSensorUpdate(HdRobotLidarSensorUpdate update);
  void EnqueueHeightScanSensorUpdate(HdRobotHeightScanSensorUpdate update);
  void ApplyPendingUpdates();

  std::optional<HydraMaterial> GetMaterialDataCopy(HdRobotMaterialHandle handle) const;
  int ResolveMaterialIndexForUpload(HdRobotMaterialHandle handle) const;

  std::vector<HdRobotMeshRecord> GetMeshRecordsSnapshot() const;
  std::vector<HdRobotMaterialRecord> GetMaterialRecordsSnapshot() const;
  std::vector<HydraLight> GetActiveLightSnapshot() const;
  std::vector<HdRobotCameraData> GetActiveCameraSnapshot() const;
  std::vector<HdRobotLidarSensorData> GetActiveLidarSensorSnapshot() const;
  std::vector<HdRobotHeightScanSensorData> GetActiveHeightScanSensorSnapshot() const;

  std::vector<HdRobotMeshRecord> ConsumeDirtyMeshGeometry();
  std::vector<HdRobotMeshRecord> ConsumeDirtyMeshInstances();
  std::vector<uint32_t> ConsumeDirtyMaterialIndices();
  void SetMeshRendererState(uint32_t meshIndex, int rendererMeshId, std::vector<int> rendererInstanceIds);

private:
  void _ApplyMeshUpdates(std::vector<HdRobotMeshUpdate>& updates);
  void _ApplyMaterialUpdates(std::vector<HdRobotMaterialUpdate>& updates);
  void _ApplyLightUpdates(std::vector<HdRobotLightUpdate>& updates);
  void _ApplyCameraUpdates(std::vector<HdRobotCameraUpdate>& updates);
  void _ApplyLidarSensorUpdates(std::vector<HdRobotLidarSensorUpdate>& updates);
  void _ApplyHeightScanSensorUpdates(std::vector<HdRobotHeightScanSensorUpdate>& updates);

  mutable std::mutex _sceneMutex;
  HdRobotSlotVector<HdRobotMeshRecord, HdRobotMeshHandle> _meshes;
  HdRobotSlotVector<HdRobotMaterialRecord, HdRobotMaterialHandle> _materials;
  HdRobotSlotVector<HdRobotLightRecord, HdRobotLightHandle> _lights;
  HdRobotSlotVector<HdRobotCameraRecord, HdRobotCameraHandle> _cameras;
  HdRobotSlotVector<HdRobotLidarSensorRecord, HdRobotLidarSensorHandle> _lidarSensors;
  HdRobotSlotVector<HdRobotHeightScanSensorRecord, HdRobotHeightScanSensorHandle> _heightScanSensors;
  HdRobotMaterialHandle _defaultMaterialHandle;

  std::mutex _pendingMutex;
  std::vector<HdRobotMeshUpdate> _pendingMeshUpdates;
  std::vector<HdRobotMaterialUpdate> _pendingMaterialUpdates;
  std::vector<HdRobotLightUpdate> _pendingLightUpdates;
  std::vector<HdRobotCameraUpdate> _pendingCameraUpdates;
  std::vector<HdRobotLidarSensorUpdate> _pendingLidarSensorUpdates;
  std::vector<HdRobotHeightScanSensorUpdate> _pendingHeightScanSensorUpdates;
};

PXR_NAMESPACE_CLOSE_SCOPE
