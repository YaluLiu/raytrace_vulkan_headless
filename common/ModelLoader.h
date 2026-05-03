#pragma once
#include <cstdint>
#include <vector>
#include <string>
#include <glm/glm.hpp>
#include "data_loader.h"
#include <iostream>

struct TextureAsset
{
  std::string          sourcePath;
  std::vector<uint8_t> encodedBytes;
};

class ModelLoader
{
public:
  // Pure virtual function to enforce implementation in derived classes
  void loadModel(const std::string& filename) {};
  ~ModelLoader() = default;  // Virtual destructor for proper cleanup
  // Common member variables
  std::vector<VertexObj>   m_vertices;
  std::vector<uint32_t>    m_indices;
  std::vector<MaterialObj> m_materials;
  std::vector<std::string> m_textures;
  std::vector<TextureAsset> m_textureAssets;
  std::vector<int32_t>     m_matIndx;
};
