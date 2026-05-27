#pragma once

#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/usd/sdf/path.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "camera.h"
#include "heightScanSensor.h"
#include "lidarSensor.h"
#include "sceneData.h"

PXR_NAMESPACE_OPEN_SCOPE

struct HdRobotTileConfig
{
  bool     enabled = false;
  bool     colorEnabled = true;
  bool     depthEnabled = true;
  uint32_t cameraWidth = 1920;
  uint32_t cameraHeight = 1080;
  uint32_t gridColumns = 10;
  uint32_t gridRows = 10;
};

struct HdRobotLidarVisualizationConfig
{
  bool enabled = false;
  uint32_t sensorIndex = 0;
  float pointSizePixels = 2.0f;
};

struct HdRobotHeightScanVisualizationConfig
{
  bool enabled = false;
  uint32_t sensorIndex = 0;
  float pointSizePixels = 2.0f;
};

class HdRobotRenderParam final : public HdRenderParam
{
 public:
  int RegisterTexturePath(const std::string& texturePath, TextureUsage usage = TextureUsage::BaseColor);
  const std::vector<std::string>& GetTexturePaths() const;
  const std::vector<TextureAsset>& GetTextureAssets() const;
  uint64_t GetTextureRegistryVersion() const;
  void UpsertCamera(const HdRobotCameraData& cameraData);
  void UpsertLidarSensor(const HdRobotLidarSensorData& sensorData);
  void UpsertHeightScanSensor(const HdRobotHeightScanSensorData& sensorData);
  void RemoveCamera(const SdfPath& cameraId);
  void RemoveLidarSensor(const SdfPath& sensorId);
  void RemoveHeightScanSensor(const SdfPath& sensorId);
  std::vector<HdRobotCameraData> GetCamerasSnapshot() const;
  std::vector<HdRobotLidarSensorData> GetLidarSensorsSnapshot() const;
  std::vector<HdRobotHeightScanSensorData> GetHeightScanSensorsSnapshot() const;
  void SetTileConfig(const HdRobotTileConfig& config);
  HdRobotTileConfig GetTileConfig() const;
  void SetLidarVisualizationConfig(const HdRobotLidarVisualizationConfig& config);
  HdRobotLidarVisualizationConfig GetLidarVisualizationConfig() const;
  void SetHeightScanVisualizationConfig(const HdRobotHeightScanVisualizationConfig& config);
  HdRobotHeightScanVisualizationConfig GetHeightScanVisualizationConfig() const;

  void MarkAllMeshesInstanceDirty();
  void MarkMeshInstanceDirty(size_t meshId);
  void MarkMeshGeometryDirty(size_t meshId);
  bool ConsumeMeshInstanceDirty(size_t meshId);
  bool ConsumeMeshGeometryDirty(size_t meshId);
  void MarkMaterialDirty(size_t materialId);
  bool IsMaterialDirty(size_t materialId) const;
  void ClearAllMaterialDirty();

 public:
  // Shared render state synchronized by Hydra prims and consumed by render pass.
  mutable std::mutex mutex;
  std::vector<HydraMesh> v_mesh;
  std::vector<HydraMaterial> v_mat;
  std::vector<HydraLight> v_light;
  std::vector<HdRobotCameraData> v_camera;
  std::vector<HdRobotLidarSensorData> v_lidarSensor;
  std::vector<HdRobotHeightScanSensorData> v_heightScanSensor;
  HdRobotTileConfig tileConfig;
  HdRobotLidarVisualizationConfig lidarVisualizationConfig;
  HdRobotHeightScanVisualizationConfig heightScanVisualizationConfig;
  TextureRegistry textureRegistry;

};

PXR_NAMESPACE_CLOSE_SCOPE
