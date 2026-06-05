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

namespace hdrobot {

struct MeshRecord
{
  uint32_t sceneIndex = UINT32_MAX;
  SdfPath id;
  HydraMesh data;
  bool active = true;
  bool geometryDirty = true;
  bool instanceDirty = true;
};

template <typename DataT>
struct SceneRecord
{
  using DataType = DataT;

  SdfPath id;
  DataT data;
  bool active = true;
  bool dirty = true;
};

using MaterialRecord = SceneRecord<HydraMaterial>;
using LightRecord = SceneRecord<HydraLight>;
using CameraRecord = SceneRecord<CameraData>;
using LidarSensorRecord = SceneRecord<LidarSensorData>;
using HeightScanSensorRecord = SceneRecord<HeightScanSensorData>;

struct MeshUpdate
{
  MeshHandle handle;
  std::optional<HydraMesh> data;
  std::optional<bool> active;
  bool geometryDirty = false;
  bool instanceDirty = false;
};

template <typename HandleT, typename DataT>
struct SceneUpdate
{
  HandleT handle;
  DataT data;
  bool active = true;
  bool dirty = true;
};

using MaterialUpdate = SceneUpdate<MaterialHandle, HydraMaterial>;
using LightUpdate = SceneUpdate<LightHandle, HydraLight>;
using CameraUpdate = SceneUpdate<CameraHandle, CameraData>;
using LidarSensorUpdate = SceneUpdate<LidarSensorHandle, LidarSensorData>;
using HeightScanSensorUpdate =
    SceneUpdate<HeightScanSensorHandle, HeightScanSensorData>;

class SceneStore
{
public:
  SceneStore();

  MeshHandle CreateMesh(const SdfPath& id);
  MaterialHandle CreateMaterial(const SdfPath& id);
  LightHandle CreateLight(const SdfPath& id);
  CameraHandle CreateCamera(const SdfPath& id);
  LidarSensorHandle CreateLidarSensor(const SdfPath& id);
  HeightScanSensorHandle CreateHeightScanSensor(const SdfPath& id);

  MaterialHandle GetDefaultMaterialHandle() const;

  void DestroyMesh(MeshHandle handle);
  void DestroyMaterial(MaterialHandle handle);
  void DestroyLight(LightHandle handle);
  void DestroyCamera(CameraHandle handle);
  void DestroyLidarSensor(LidarSensorHandle handle);
  void DestroyHeightScanSensor(HeightScanSensorHandle handle);

  void EnqueueMeshUpdate(MeshUpdate update);
  void EnqueueMaterialUpdate(MaterialUpdate update);
  void EnqueueLightUpdate(LightUpdate update);
  void EnqueueCameraUpdate(CameraUpdate update);
  void EnqueueLidarSensorUpdate(LidarSensorUpdate update);
  void EnqueueHeightScanSensorUpdate(HeightScanSensorUpdate update);
  void ApplyPendingUpdates();

  int RegisterTexturePath(const std::string& texturePath, TextureUsage usage = TextureUsage::BaseColor);
  std::vector<TextureAsset> GetTextureAssetsSnapshot() const;
  uint64_t GetTextureRegistryVersion() const;

  std::optional<HydraMaterial> GetMaterialDataCopy(MaterialHandle handle) const;
  int ResolveMaterialIndexForUpload(MaterialHandle handle) const;

  std::vector<MeshRecord> GetMeshRecordsSnapshot() const;
  std::vector<MaterialRecord> GetMaterialRecordsSnapshot() const;
  std::vector<HydraLight> GetActiveLightSnapshot() const;
  std::vector<CameraData> GetActiveCameraSnapshot() const;
  std::vector<LidarSensorData> GetActiveLidarSensorSnapshot() const;
  std::vector<HeightScanSensorData> GetActiveHeightScanSensorSnapshot() const;

  std::vector<MeshRecord> ConsumeDirtyMeshGeometry();
  std::vector<MeshRecord> ConsumeDirtyMeshInstances();
  std::vector<uint32_t> ConsumeDirtyMaterialIndices();

private:
  void _ApplyMeshUpdates(std::vector<MeshUpdate>& updates);

  mutable std::mutex _sceneMutex;
  mutable std::mutex _textureRegistryMutex;
  SlotVector<MeshRecord, MeshHandle> _meshes;
  SlotVector<MaterialRecord, MaterialHandle> _materials;
  SlotVector<LightRecord, LightHandle> _lights;
  SlotVector<CameraRecord, CameraHandle> _cameras;
  SlotVector<LidarSensorRecord, LidarSensorHandle> _lidarSensors;
  SlotVector<HeightScanSensorRecord, HeightScanSensorHandle> _heightScanSensors;
  MaterialHandle _defaultMaterialHandle;
  TextureRegistry _textureRegistry;

  std::mutex _pendingMutex;
  std::vector<MeshUpdate> _pendingMeshUpdates;
  std::vector<MaterialUpdate> _pendingMaterialUpdates;
  std::vector<LightUpdate> _pendingLightUpdates;
  std::vector<CameraUpdate> _pendingCameraUpdates;
  std::vector<LidarSensorUpdate> _pendingLidarSensorUpdates;
  std::vector<HeightScanSensorUpdate> _pendingHeightScanSensorUpdates;
};

} // namespace hdrobot

PXR_NAMESPACE_CLOSE_SCOPE
