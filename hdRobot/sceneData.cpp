#include "sceneData.h"

PXR_NAMESPACE_OPEN_SCOPE

void HydraMaterial::set_default()
{
  diffuse = glm::vec3(0.18f, 0.18f, 0.18f);
  baseColorFactor = diffuse;
  emission = glm::vec3(0.0f, 0.0f, 0.0f);
  emissionFactor = emission;
  transmissionColorFactor = glm::vec3(1.0f, 1.0f, 1.0f);
  subsurfaceColorFactor = glm::vec3(1.0f, 1.0f, 1.0f);
  metallicFactor = 0.0f;
  roughnessFactor = 0.5f;
  opacityFactor = 1.0f;
  transmissionFactor = 0.0f;
  subsurfaceFactor = 0.0f;
  subsurfaceScale = 0.0f;
  diffuseTextureId = -1;
  baseColorTextureId = -1;
  metallicTextureId = -1;
  roughnessTextureId = -1;
  normalTextureId = -1;
  emissionTextureId = -1;
  opacityTextureId = -1;
  subsurfaceTextureId = -1;
  texturePath.clear();
  material_changed = true;
}

namespace
{
std::string TextureRegistryKey(const std::string &texturePath, TextureUsage usage)
{
  return texturePath + "#" + std::to_string(static_cast<int>(usage));
}
}  // namespace

TextureColorSpace TextureColorSpaceForUsage(TextureUsage usage)
{
  switch (usage)
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

int TextureRegistry::Register(const std::string &texturePath, TextureUsage usage)
{
  const std::string key = TextureRegistryKey(texturePath, usage);
  const auto it = _textureIdByPath.find(key);
  if (it != _textureIdByPath.end())
  {
    return it->second;
  }

  const int textureId = static_cast<int>(_texturePaths.size());
  _texturePaths.emplace_back(texturePath);
  TextureAsset textureAsset;
  textureAsset.sourcePath = texturePath;
  textureAsset.usage = usage;
  textureAsset.colorSpace = TextureColorSpaceForUsage(usage);
  _textureAssets.emplace_back(std::move(textureAsset));
  _textureIdByPath.emplace(key, textureId);
  ++_version;
  return textureId;
}

const std::vector<std::string> &TextureRegistry::GetPaths() const
{
  return _texturePaths;
}

const std::vector<TextureAsset> &TextureRegistry::GetTextureAssets() const
{
  return _textureAssets;
}

uint64_t TextureRegistry::GetVersion() const
{
  return _version;
}

PXR_NAMESPACE_CLOSE_SCOPE
