#pragma once

#include "camera.h"
#include "sceneData.h"

#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/usd/sdf/path.h>

#include <mutex>
#include <string>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRenderParam final : public HdRenderParam
{
public:
  int RegisterTexturePath(const std::string& texturePath);
  const std::vector<std::string>& GetTexturePaths() const;

  void UpdateLidarCamera(const SdfPath& cameraId, const HdRobotLidarData& lidarData);
  void ClearLidarCamera(const SdfPath& cameraId);
  bool GetLidarCamera(HdRobotLidarData* lidarData) const;

  void MarkAllMeshesTlasDirty();
  void MarkMeshTlasDirty(size_t meshId);
  void MarkMeshBlasDirty(size_t meshId);
  bool ConsumeMeshTlasDirty(size_t meshId);
  bool ConsumeMeshBlasDirty(size_t meshId);
  void MarkMaterialDirty(size_t materialId);
  bool IsMaterialDirty(size_t materialId) const;
  void ClearAllMaterialDirty();

public:
  // Shared render state synchronized by Hydra prims and consumed by render pass.
  std::mutex                mutex;
  std::vector<HydraMesh>    v_mesh;
  std::vector<HydraMaterial> v_mat;
  std::vector<HydraLight>   v_light;
  std::vector<Sphere>       v_sphere;
  TextureRegistry           textureRegistry;

private:
  mutable std::mutex _lidarMutex;
  SdfPath            _lidarCameraId;
  HdRobotLidarData   _lidarCameraData;
  bool               _hasLidarCamera{ false };
  bool               _warnedMultipleLidarCameras{ false };
};

PXR_NAMESPACE_CLOSE_SCOPE
