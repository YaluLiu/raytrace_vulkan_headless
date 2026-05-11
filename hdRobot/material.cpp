#include "material.h"

#include "materialXParser.h"

#include "pxr/imaging/hd/material.h"
#include "pxr/imaging/hd/sceneDelegate.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
int RegisterTexture(HdRobotRenderParam& scene, const std::string& texturePath, TextureUsage usage)
{
  if (texturePath.empty())
  {
    return -1;
  }
  return scene.RegisterTexturePath(texturePath, usage);
}
}  // namespace

HdRobotMaterial::HdRobotMaterial(const SdfPath& id, HdRobotRenderParam& scene) : HdMaterial(id), _scene(scene)
{
  std::lock_guard guard(_scene.mutex);
  _mat_id = _scene.v_mat.size();
  _scene.v_mat.emplace_back(HydraMaterial());
}

void HdRobotMaterial::Finalize(HdRenderParam* renderParam)
{
  _scene.v_mat[_mat_id].set_default();
  _scene.MarkMaterialDirty(_mat_id);
}

HdDirtyBits HdRobotMaterial::GetInitialDirtyBitsMask() const
{
  return DirtyBits::DirtyParams;
}

void HdRobotMaterial::Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits)
{
  if (!TF_VERIFY(sceneDelegate)) return;
  bool pullMaterial = (*dirtyBits & DirtyBits::DirtyParams);

  *dirtyBits = DirtyBits::Clean;

  if (!pullMaterial)
  {
    return;
  }

  _baseColorPrimvarName = TfToken();
  _scene.v_mat[_mat_id].set_default();
  const SdfPath& id = GetId();
  const VtValue& resource = sceneDelegate->GetMaterialResource(id);

  if (!resource.IsHolding<HdMaterialNetworkMap>())
  {
    return;
  }

  const HdMaterialNetworkMap& networkMap = resource.UncheckedGet<HdMaterialNetworkMap>();
  bool isVolume = false;

  HdMaterialNetwork2 network = HdConvertToHdMaterialNetwork2(networkMap, &isVolume);
  if (isVolume)
  {
    TF_WARN("Volume %s unsupported", id.GetText());
    return;
  }

  MaterialXParseResult result = ParseMaterialXNetwork(network, _scene.v_mat[_mat_id]);
  HydraMaterial& material = _scene.v_mat[_mat_id];
  material = result.material;
  _baseColorPrimvarName = result.baseColorPrimvarName;

  for (const MaterialXTextureBinding& binding : result.textures)
  {
    const int textureId = RegisterTexture(_scene, binding.assetPath, binding.usage);
    if (textureId >= 0)
    {
      ApplyMaterialXTextureId(material, binding.usage, textureId);
      _scene.MarkMaterialDirty(_mat_id);
    }
  }

  if (result.hasMaterialOpinion)
  {
    _scene.MarkMaterialDirty(_mat_id);
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
