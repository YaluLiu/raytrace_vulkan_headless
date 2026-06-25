#pragma once

#include "training_scene.h"

#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usd/stage.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace headless_training
{

TextureColorSpace TextureColorSpaceForUsage(TextureUsage usage);

class TextureRegistry
{
public:
  explicit TextureRegistry(PXR_NS::UsdStageRefPtr stage);

  int Register(const PXR_NS::SdfAssetPath& assetPath, TextureUsage usage);
  std::vector<TextureAsset> TakeAssets();

private:
  PXR_NS::UsdStageRefPtr m_stage;
  std::unordered_map<std::string, int> m_textureIdByKey;
  std::vector<TextureAsset> m_textureAssets;

  void ExportTextureBytes(const PXR_NS::SdfAssetPath& assetPath, TextureAsset& textureAsset) const;
};

} // namespace headless_training
