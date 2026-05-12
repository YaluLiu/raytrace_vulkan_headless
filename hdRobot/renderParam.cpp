#include "renderParam.h"

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
