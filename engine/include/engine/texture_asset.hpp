#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class TextureUsage
{
  Unknown,
  BaseColor,
  Metallic,
  Roughness,
  Normal,
  Emission,
  Opacity,
  Subsurface,
  Light
};

enum class TextureColorSpace
{
  SRGB,
  Linear
};

struct TextureAsset
{
  std::string sourcePath;
  std::vector<uint8_t> encodedBytes;
  TextureUsage usage = TextureUsage::Unknown;
  TextureColorSpace colorSpace = TextureColorSpace::SRGB;
};
