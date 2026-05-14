#pragma once

#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/usd/sdf/path.h>

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "camera.h"
#include "sceneData.h"

PXR_NAMESPACE_OPEN_SCOPE

struct HdRobotTileConfig
{
  bool     enabled = false;
  uint32_t cameraWidth = 100;
  uint32_t cameraHeight = 100;
  uint32_t gridColumns = 3;
  uint32_t gridRows = 3;
};

class HdRobotRenderParam final : public HdRenderParam
{
 public:
  int RegisterTexturePath(const std::string& texturePath, TextureUsage usage = TextureUsage::BaseColor);
  const std::vector<std::string>& GetTexturePaths() const;
  const std::vector<TextureAsset>& GetTextureAssets() const;
  uint64_t GetTextureRegistryVersion() const;
  void UpsertCamera(const HdRobotCameraData& cameraData);
  void RemoveCamera(const SdfPath& cameraId);
  std::vector<HdRobotCameraData> GetCamerasSnapshot() const;
  void SetTileConfig(const HdRobotTileConfig& config);
  HdRobotTileConfig GetTileConfig() const;

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
  HdRobotTileConfig tileConfig;
  TextureRegistry textureRegistry;

};

PXR_NAMESPACE_CLOSE_SCOPE
