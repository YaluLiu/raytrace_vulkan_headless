#include "renderParam.h"

#include <algorithm>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
template <typename T>
void RemoveByName(std::vector<T>& items, const std::string& name)
{
  items.erase(std::remove_if(items.begin(), items.end(), [&name](const T& item) {
                  return item.name == name;
                }),
                items.end());
}

template <typename T>
void UpsertByName(std::vector<T>& items, const T& item)
{
  auto itemIt = std::find_if(items.begin(), items.end(), [&item](const T& existingItem) {
    return existingItem.name == item.name;
  });
  if(itemIt == items.end())
  {
    items.push_back(item);
    return;
  }
  *itemIt = item;
}
} // namespace

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
  UpsertByName(v_camera, cameraData);
}

void HdRobotRenderParam::UpsertLidarSensor(const HdRobotLidarSensorData& sensorData)
{
  std::lock_guard guard(mutex);
  UpsertByName(v_lidarSensor, sensorData);
}

void HdRobotRenderParam::RemoveCamera(const SdfPath& cameraId)
{
  const std::string cameraName = cameraId.GetString();
  std::lock_guard   guard(mutex);
  RemoveByName(v_camera, cameraName);
  RemoveByName(v_lidarSensor, cameraName);
}

void HdRobotRenderParam::RemoveLidarSensor(const SdfPath& sensorId)
{
  const std::string sensorName = sensorId.GetString();
  std::lock_guard   guard(mutex);
  RemoveByName(v_lidarSensor, sensorName);
}

std::vector<HdRobotCameraData> HdRobotRenderParam::GetCamerasSnapshot() const
{
  std::lock_guard guard(mutex);
  return v_camera;
}

std::vector<HdRobotLidarSensorData> HdRobotRenderParam::GetLidarSensorsSnapshot() const
{
  std::lock_guard guard(mutex);
  return v_lidarSensor;
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

void HdRobotRenderParam::SetLidarVisualizationConfig(const HdRobotLidarVisualizationConfig& config)
{
  std::lock_guard guard(mutex);
  lidarVisualizationConfig = config;
}

HdRobotLidarVisualizationConfig HdRobotRenderParam::GetLidarVisualizationConfig() const
{
  std::lock_guard guard(mutex);
  return lidarVisualizationConfig;
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
