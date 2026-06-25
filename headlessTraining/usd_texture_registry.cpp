#include "usd_texture_registry.h"

#include <pxr/usd/ar/asset.h>
#include <pxr/usd/ar/resolvedPath.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/ar/resolverContextBinder.h>

#include <cstdint>
#include <iostream>
#include <memory>
#include <utility>

namespace headless_training
{
namespace
{

std::string TextureRegistryKey(const std::string& texturePath, TextureUsage usage)
{
  return texturePath + "#" + std::to_string(static_cast<int>(usage));
}

} // namespace

TextureColorSpace TextureColorSpaceForUsage(TextureUsage usage)
{
  switch(usage)
  {
  case TextureUsage::Metallic:
  case TextureUsage::Roughness:
  case TextureUsage::Normal:
  case TextureUsage::Opacity:
  case TextureUsage::Subsurface:
    return TextureColorSpace::Linear;
  case TextureUsage::Unknown:
  case TextureUsage::BaseColor:
  case TextureUsage::Emission:
  case TextureUsage::Light:
  default:
    return TextureColorSpace::SRGB;
  }
}

TextureRegistry::TextureRegistry(PXR_NS::UsdStageRefPtr stage) : m_stage(std::move(stage)) {}

int TextureRegistry::Register(const PXR_NS::SdfAssetPath& assetPath, TextureUsage usage)
{
  const std::string authoredPath = assetPath.GetAssetPath();
  const std::string resolvedPath = assetPath.GetResolvedPath();
  const std::string sourcePath = !resolvedPath.empty() ? resolvedPath : authoredPath;
  if(sourcePath.empty())
  {
    return -1;
  }

  const std::string key = TextureRegistryKey(sourcePath, usage);
  const auto it = m_textureIdByKey.find(key);
  if(it != m_textureIdByKey.end())
  {
    return it->second;
  }

  TextureAsset textureAsset;
  textureAsset.sourcePath = sourcePath;
  textureAsset.usage = usage;
  textureAsset.colorSpace = TextureColorSpaceForUsage(usage);
  ExportTextureBytes(assetPath, textureAsset);

  const int textureId = static_cast<int>(m_textureAssets.size());
  m_textureAssets.emplace_back(std::move(textureAsset));
  m_textureIdByKey.emplace(key, textureId);
  return textureId;
}

std::vector<TextureAsset> TextureRegistry::TakeAssets()
{
  return std::move(m_textureAssets);
}

void TextureRegistry::ExportTextureBytes(const PXR_NS::SdfAssetPath& assetPath, TextureAsset& textureAsset) const
{
  PXR_NS::ArResolverContextBinder contextBinder(m_stage->GetPathResolverContext());
  PXR_NS::ArResolver& resolver = PXR_NS::ArGetResolver();

  PXR_NS::ArResolvedPath resolvedPath;
  if(!assetPath.GetResolvedPath().empty())
  {
    resolvedPath = PXR_NS::ArResolvedPath(assetPath.GetResolvedPath());
  }
  if(!resolvedPath)
  {
    resolvedPath = resolver.Resolve(assetPath.GetAssetPath());
  }
  if(!resolvedPath)
  {
    std::cerr << "[robot_training_headless] Warning: failed to resolve "
                 "texture asset "
              << assetPath.GetAssetPath() << '\n';
    return;
  }

  textureAsset.sourcePath = resolvedPath.GetPathString();
  std::shared_ptr<PXR_NS::ArAsset> asset = resolver.OpenAsset(resolvedPath);
  if(!asset)
  {
    std::cerr << "[robot_training_headless] Warning: failed to open texture asset " << resolvedPath.GetPathString()
              << '\n';
    return;
  }

  const auto buffer = asset->GetBuffer();
  const auto size = asset->GetSize();
  if(!buffer || size == 0)
  {
    std::cerr << "[robot_training_headless] Warning: empty texture asset " << resolvedPath.GetPathString() << '\n';
    return;
  }

  const auto* bytes = reinterpret_cast<const uint8_t*>(buffer.get());
  textureAsset.encodedBytes.assign(bytes, bytes + size);
}

} // namespace headless_training
