#pragma once
#include <cstdint>
#include <glm/glm.hpp>
#include <iostream>
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
  // Pure virtual function to enforce implementation in derived classes
  void loadModel(const std::string& filename) {};
  ~ModelLoader() = default;  // Virtual destructor for proper cleanup
  // Common member variables
  std::vector<VertexObj> m_vertices;
  std::vector<uint32_t> m_indices;
  std::vector<MaterialObj> m_materials;
  std::vector<std::string> m_textures;
  std::vector<TextureAsset> m_textureAssets;
  std::vector<int32_t> m_matIndx;
};
