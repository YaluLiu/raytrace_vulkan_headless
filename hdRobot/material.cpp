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

HdRobotMaterial::HdRobotMaterial(const SdfPath& id, HdRobotRenderParam& scene, HdRobotMaterialHandle handle)
    : HdMaterial(id)
    , _scene(scene)
    , _handle(handle)
{
  _materialData.set_default();
}

void HdRobotMaterial::Finalize(HdRenderParam* renderParam)
{
  TF_UNUSED(renderParam);
  _scene.GetBackendScene().DestroyMaterial(_handle);
}

HdDirtyBits HdRobotMaterial::GetInitialDirtyBitsMask() const
{
  return DirtyBits::DirtyParams;
}

HdRobotMaterialHandle HdRobotMaterial::GetHandle() const
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
    _scene.GetBackendScene().EnqueueMaterialUpdate(HdRobotMaterialUpdate{_handle, _materialData});
    return;
  }

  const HdMaterialNetworkMap& networkMap = resource.UncheckedGet<HdMaterialNetworkMap>();
  bool isVolume = false;

  HdMaterialNetwork2 network = HdConvertToHdMaterialNetwork2(networkMap, &isVolume);
  if (isVolume)
  {
    TF_WARN("Volume %s unsupported", id.GetText());
    _materialData = material;
    _scene.GetBackendScene().EnqueueMaterialUpdate(HdRobotMaterialUpdate{_handle, _materialData});
    return;
  }

  MaterialXParseResult result = ParseMaterialXNetwork(network, material);
  material = result.material;
  _baseColorPrimvarName = result.baseColorPrimvarName;

  for (const MaterialXTextureBinding& binding : result.textures)
  {
    const int textureId = RegisterTexture(_scene, binding.assetPath, binding.usage);
    if (textureId >= 0)
    {
      ApplyMaterialXTextureId(material, binding.usage, textureId);
    }
  }

  _materialData = material;
  _scene.GetBackendScene().EnqueueMaterialUpdate(HdRobotMaterialUpdate{_handle, _materialData});
}

PXR_NAMESPACE_CLOSE_SCOPE
