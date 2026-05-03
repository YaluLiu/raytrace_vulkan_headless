#include "renderParam.h"

#include <pxr/base/tf/diagnostic.h>

PXR_NAMESPACE_OPEN_SCOPE

int HdRobotRenderParam::RegisterTexturePath(const std::string& texturePath, TextureUsage usage)
{
  std::lock_guard guard(mutex);
  return textureRegistry.Register(texturePath, usage);
}

const std::vector<std::string>& HdRobotRenderParam::GetTexturePaths() const
{
  return textureRegistry.GetPaths();
}

const std::vector<TextureAsset>& HdRobotRenderParam::GetTextureAssets() const
{
  return textureRegistry.GetTextureAssets();
}

void HdRobotRenderParam::UpdateLidarCamera(const SdfPath& cameraId, const HdRobotLidarData& lidarData)
{
  std::lock_guard<std::mutex> lock(_lidarMutex);

  if (_hasLidarCamera && _lidarCameraId != cameraId && !_warnedMultipleLidarCameras)
  {
    TF_WARN("Multiple lidar cameras are unsupported (%s -> %s). Latest synced camera wins.", _lidarCameraId.GetText(),
            cameraId.GetText());
    _warnedMultipleLidarCameras = true;
  }

  _lidarCameraId = cameraId;
  _lidarCameraData = lidarData;
  _hasLidarCamera = true;
}

void HdRobotRenderParam::ClearLidarCamera(const SdfPath& cameraId)
{
  std::lock_guard<std::mutex> lock(_lidarMutex);
  if (_hasLidarCamera && _lidarCameraId == cameraId)
  {
    _lidarCameraId = SdfPath();
    _lidarCameraData = HdRobotLidarData();
    _hasLidarCamera = false;
    _warnedMultipleLidarCameras = false;
  }
}

bool HdRobotRenderParam::GetLidarCamera(HdRobotLidarData* lidarData) const
{
  std::lock_guard<std::mutex> lock(_lidarMutex);
  if (!_hasLidarCamera || lidarData == nullptr)
  {
    return false;
  }

  *lidarData = _lidarCameraData;
  return true;
}

void HdRobotRenderParam::MarkAllMeshesTlasDirty()
{
  for (auto& mesh : v_mesh)
  {
    mesh.tlas_changed = true;
  }
}

void HdRobotRenderParam::MarkMeshTlasDirty(size_t meshId)
{
  if (meshId < v_mesh.size())
  {
    v_mesh[meshId].tlas_changed = true;
  }
}

void HdRobotRenderParam::MarkMeshBlasDirty(size_t meshId)
{
  if (meshId < v_mesh.size())
  {
    v_mesh[meshId].blas_changed = true;
  }
}

bool HdRobotRenderParam::ConsumeMeshTlasDirty(size_t meshId)
{
  if (meshId >= v_mesh.size() || !v_mesh[meshId].tlas_changed)
  {
    return false;
  }
  v_mesh[meshId].tlas_changed = false;
  return true;
}

bool HdRobotRenderParam::ConsumeMeshBlasDirty(size_t meshId)
{
  if (meshId >= v_mesh.size() || !v_mesh[meshId].blas_changed)
  {
    return false;
  }
  v_mesh[meshId].blas_changed = false;
  return true;
}

void HdRobotRenderParam::MarkMaterialDirty(size_t materialId)
{
  if (materialId < v_mat.size())
  {
    v_mat[materialId].material_changed = true;
  }
}

bool HdRobotRenderParam::IsMaterialDirty(size_t materialId) const
{
  return materialId < v_mat.size() && v_mat[materialId].material_changed;
}

void HdRobotRenderParam::ClearAllMaterialDirty()
{
  for (auto& material : v_mat)
  {
    material.material_changed = false;
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
