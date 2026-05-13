#pragma once

#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/usd/sdf/path.h>

#include <mutex>
#include <string>
#include <vector>

#include "camera.h"
#include "sceneData.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRenderParam final : public HdRenderParam
{
 public:
  int RegisterTexturePath(const std::string& texturePath, TextureUsage usage = TextureUsage::BaseColor);
  const std::vector<std::string>& GetTexturePaths() const;
  const std::vector<TextureAsset>& GetTextureAssets() const;
  uint64_t GetTextureRegistryVersion() const;

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
  std::mutex mutex;
  std::vector<HydraMesh> v_mesh;
  std::vector<HydraMaterial> v_mat;
  std::vector<HydraLight> v_light;
  TextureRegistry textureRegistry;

};

PXR_NAMESPACE_CLOSE_SCOPE
