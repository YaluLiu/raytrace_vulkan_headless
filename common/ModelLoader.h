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

struct RasterMeshGeometry
{
  std::vector<RasterVertex> vertices;
  std::vector<uint32_t> indices;
  std::vector<int32_t> materialIndices;
};

struct RasterMeshUpload : RasterMeshGeometry
{
  std::vector<RasterMaterial> materials;
};

using ModelLoader = RasterMeshUpload;
