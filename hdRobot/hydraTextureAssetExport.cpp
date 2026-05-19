#include "hydraTextureAssetExport.h"

#include <pxr/usd/ar/asset.h>
#include <pxr/usd/ar/resolvedPath.h>
#include <pxr/usd/ar/resolver.h>

#include <cstdint>
#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
TextureAsset ExportResolvedTextureAsset(const TextureAsset &registeredTexture)
{
  TextureAsset textureAsset;
  textureAsset.sourcePath = registeredTexture.sourcePath;
  textureAsset.usage = registeredTexture.usage;
  textureAsset.colorSpace = registeredTexture.colorSpace;

  ArResolver &resolver = ArGetResolver();
  ArResolvedPath resolvedPath = resolver.Resolve(textureAsset.sourcePath);
  if(!resolvedPath)
  {
    std::cout << "[HydraTextureAssetExport]:" << textureAsset.sourcePath << " is not valid" << std::endl;
    return textureAsset;
  }

  auto asset = resolver.OpenAsset(resolvedPath);
  if(!asset)
  {
    std::cout << "[HydraTextureAssetExport]:" << resolvedPath.GetPathString() << " failed" << std::endl;
    return textureAsset;
  }

  const auto buffer = asset->GetBuffer();
  const auto size = asset->GetSize();
  if(!buffer || size == 0)
  {
    std::cout << "[HydraTextureAssetExport]:" << resolvedPath.GetPathString() << " empty texture asset" << std::endl;
    return textureAsset;
  }

  const auto *bytes = reinterpret_cast<const uint8_t *>(buffer.get());
  textureAsset.encodedBytes.assign(bytes, bytes + size);
  return textureAsset;
}
} // namespace

std::vector<TextureAsset> ExportRegisteredTextures(const std::vector<TextureAsset> &registeredTextures)
{
  std::vector<TextureAsset> exportedTextures;
  exportedTextures.reserve(registeredTextures.size());
  for(const TextureAsset &registeredTexture : registeredTextures)
  {
    exportedTextures.push_back(ExportResolvedTextureAsset(registeredTexture));
  }
  return exportedTextures;
}

PXR_NAMESPACE_CLOSE_SCOPE
