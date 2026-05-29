#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "data_loader.h"

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

class ModelLoader
{
 public:
  std::vector<VertexObj> m_vertices;
  std::vector<uint32_t> m_indices;
  std::vector<MaterialObj> m_materials;
  std::vector<int32_t> m_matIndx;
};
