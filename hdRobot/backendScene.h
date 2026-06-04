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
#include <string>
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

template <typename DataT>
struct HdRobotBackendRecord
{
  using DataType = DataT;

  SdfPath id;
  DataT data;
  bool active = true;
  bool dirty = true;
};

using HdRobotMaterialRecord = HdRobotBackendRecord<HydraMaterial>;
using HdRobotLightRecord = HdRobotBackendRecord<HydraLight>;
using HdRobotCameraRecord = HdRobotBackendRecord<HdRobotCameraData>;
using HdRobotLidarSensorRecord = HdRobotBackendRecord<HdRobotLidarSensorData>;
using HdRobotHeightScanSensorRecord = HdRobotBackendRecord<HdRobotHeightScanSensorData>;

struct HdRobotMeshUpdate
{
  HdRobotMeshHandle handle;
  std::optional<HydraMesh> data;
  std::optional<bool> active;
  bool geometryDirty = false;
  bool instanceDirty = false;
};

template <typename HandleT, typename DataT>
struct HdRobotBackendUpdate
{
  HandleT handle;
  DataT data;
  bool active = true;
  bool dirty = true;
};

using HdRobotMaterialUpdate = HdRobotBackendUpdate<HdRobotMaterialHandle, HydraMaterial>;
using HdRobotLightUpdate = HdRobotBackendUpdate<HdRobotLightHandle, HydraLight>;
using HdRobotCameraUpdate = HdRobotBackendUpdate<HdRobotCameraHandle, HdRobotCameraData>;
using HdRobotLidarSensorUpdate = HdRobotBackendUpdate<HdRobotLidarSensorHandle, HdRobotLidarSensorData>;
using HdRobotHeightScanSensorUpdate =
    HdRobotBackendUpdate<HdRobotHeightScanSensorHandle, HdRobotHeightScanSensorData>;

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

  int RegisterTexturePath(const std::string& texturePath, TextureUsage usage = TextureUsage::BaseColor);
  std::vector<TextureAsset> GetTextureAssetsSnapshot() const;
  uint64_t GetTextureRegistryVersion() const;

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

  mutable std::mutex _sceneMutex;
  mutable std::mutex _textureRegistryMutex;
  HdRobotSlotVector<HdRobotMeshRecord, HdRobotMeshHandle> _meshes;
  HdRobotSlotVector<HdRobotMaterialRecord, HdRobotMaterialHandle> _materials;
  HdRobotSlotVector<HdRobotLightRecord, HdRobotLightHandle> _lights;
  HdRobotSlotVector<HdRobotCameraRecord, HdRobotCameraHandle> _cameras;
  HdRobotSlotVector<HdRobotLidarSensorRecord, HdRobotLidarSensorHandle> _lidarSensors;
  HdRobotSlotVector<HdRobotHeightScanSensorRecord, HdRobotHeightScanSensorHandle> _heightScanSensors;
  HdRobotMaterialHandle _defaultMaterialHandle;
  TextureRegistry _textureRegistry;

  std::mutex _pendingMutex;
  std::vector<HdRobotMeshUpdate> _pendingMeshUpdates;
  std::vector<HdRobotMaterialUpdate> _pendingMaterialUpdates;
  std::vector<HdRobotLightUpdate> _pendingLightUpdates;
  std::vector<HdRobotCameraUpdate> _pendingCameraUpdates;
  std::vector<HdRobotLidarSensorUpdate> _pendingLidarSensorUpdates;
  std::vector<HdRobotHeightScanSensorUpdate> _pendingHeightScanSensorUpdates;
};

PXR_NAMESPACE_CLOSE_SCOPE
