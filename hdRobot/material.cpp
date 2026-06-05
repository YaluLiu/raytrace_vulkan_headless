#include "material.h"

#include "sceneStore.h"
#include "materialXParser.h"

#include "pxr/imaging/hd/material.h"
#include "pxr/imaging/hd/sceneDelegate.h"

PXR_NAMESPACE_OPEN_SCOPE

using hdrobot::MaterialHandle;
using hdrobot::MaterialUpdate;
using hdrobot::SceneStore;

namespace
{
int RegisterTexture(SceneStore& sceneStore, const std::string& texturePath, TextureUsage usage)
{
  if (texturePath.empty())
  {
    return -1;
  }
  return sceneStore.RegisterTexturePath(texturePath, usage);
}
}  // namespace

HdRobotMaterial::HdRobotMaterial(const SdfPath& id, SceneStore& sceneStore, MaterialHandle handle)
    : HdMaterial(id)
    , _sceneStore(sceneStore)
    , _handle(handle)
{
  _materialData.set_default();
}

void HdRobotMaterial::Finalize(HdRenderParam* renderParam)
{
  TF_UNUSED(renderParam);
  _sceneStore.DestroyMaterial(_handle);
}

HdDirtyBits HdRobotMaterial::GetInitialDirtyBitsMask() const
{
  return DirtyBits::DirtyParams;
}

MaterialHandle HdRobotMaterial::GetHandle() const
{
  return _handle;
}

const HydraMaterial& HdRobotMaterial::GetMaterialData() const
{
  return _materialData;
}

const TfToken& HdRobotMaterial::GetBaseColorPrimvarName() const
{
  return _baseColorPrimvarName;
}

void HdRobotMaterial::Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits)
{
  TF_UNUSED(renderParam);
  if (!TF_VERIFY(sceneDelegate)) return;
  bool pullMaterial = (*dirtyBits & DirtyBits::DirtyParams);

  *dirtyBits = DirtyBits::Clean;

  if (!pullMaterial)
  {
    return;
  }

  _baseColorPrimvarName = TfToken();
  HydraMaterial material;
  material.set_default();
  const SdfPath& id = GetId();
  const VtValue& resource = sceneDelegate->GetMaterialResource(id);

  if (!resource.IsHolding<HdMaterialNetworkMap>())
  {
    _materialData = material;
    _sceneStore.EnqueueMaterialUpdate(MaterialUpdate{_handle, _materialData});
    return;
  }

  const HdMaterialNetworkMap& networkMap = resource.UncheckedGet<HdMaterialNetworkMap>();
  bool isVolume = false;

  HdMaterialNetwork2 network = HdConvertToHdMaterialNetwork2(networkMap, &isVolume);
  if (isVolume)
  {
    TF_WARN("Volume %s unsupported", id.GetText());
    _materialData = material;
    _sceneStore.EnqueueMaterialUpdate(MaterialUpdate{_handle, _materialData});
    return;
  }

  MaterialXParseResult result = ParseMaterialXNetwork(network, material);
  material = result.material;
  _baseColorPrimvarName = result.baseColorPrimvarName;

  for (const MaterialXTextureBinding& binding : result.textures)
  {
    const int textureId = RegisterTexture(_sceneStore, binding.assetPath, binding.usage);
    if (textureId >= 0)
    {
      ApplyMaterialXTextureId(material, binding.usage, textureId);
    }
  }

  _materialData = material;
  _sceneStore.EnqueueMaterialUpdate(MaterialUpdate{_handle, _materialData});
}

PXR_NAMESPACE_CLOSE_SCOPE
