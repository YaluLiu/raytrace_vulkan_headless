//
// Copyright (C) 2023 Pablo Delgado Krämer
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

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
