#include "renderParam.h"

#include <algorithm>

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

uint64_t HdRobotRenderParam::GetTextureRegistryVersion() const
{
  return textureRegistry.GetVersion();
}

void HdRobotRenderParam::UpsertCamera(const HdRobotCameraData& cameraData)
{
  std::lock_guard guard(mutex);
  auto cameraIt = std::find_if(v_camera.begin(), v_camera.end(), [&cameraData](const HdRobotCameraData& camera) {
    return camera.name == cameraData.name;
  });
  if(cameraIt == v_camera.end())
  {
    v_camera.push_back(cameraData);
    return;
  }
  *cameraIt = cameraData;
}

void HdRobotRenderParam::RemoveCamera(const SdfPath& cameraId)
{
  const std::string cameraName = cameraId.GetString();
  std::lock_guard   guard(mutex);
  v_camera.erase(std::remove_if(v_camera.begin(), v_camera.end(), [&cameraName](const HdRobotCameraData& camera) {
                   return camera.name == cameraName;
                 }),
                 v_camera.end());
}

std::vector<HdRobotCameraData> HdRobotRenderParam::GetCamerasSnapshot() const
{
  std::lock_guard guard(mutex);
  return v_camera;
}

void HdRobotRenderParam::SetTileConfig(const HdRobotTileConfig& config)
{
  std::lock_guard guard(mutex);
  tileConfig = config;
}

HdRobotTileConfig HdRobotRenderParam::GetTileConfig() const
{
  std::lock_guard guard(mutex);
  return tileConfig;
}

void HdRobotRenderParam::MarkAllMeshesInstanceDirty()
{
  for (auto& mesh : v_mesh)
  {
    mesh.instance_changed = true;
  }
}

void HdRobotRenderParam::MarkMeshInstanceDirty(size_t meshId)
{
  if (meshId < v_mesh.size())
  {
    v_mesh[meshId].instance_changed = true;
  }
}

void HdRobotRenderParam::MarkMeshGeometryDirty(size_t meshId)
{
  if (meshId < v_mesh.size())
  {
    v_mesh[meshId].geometry_changed = true;
  }
}

bool HdRobotRenderParam::ConsumeMeshInstanceDirty(size_t meshId)
{
  if (meshId >= v_mesh.size() || !v_mesh[meshId].instance_changed)
  {
    return false;
  }
  v_mesh[meshId].instance_changed = false;
  return true;
}

bool HdRobotRenderParam::ConsumeMeshGeometryDirty(size_t meshId)
{
  if (meshId >= v_mesh.size() || !v_mesh[meshId].geometry_changed)
  {
    return false;
  }
  v_mesh[meshId].geometry_changed = false;
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
