#pragma once

#include "sceneData.h"

#include <pxr/imaging/hd/material.h>

#include <string>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

struct MaterialXTextureBinding
{
  std::string assetPath;
  TextureUsage usage = TextureUsage::Unknown;
};

struct MaterialXParseResult
{
  HydraMaterial material;
  std::vector<MaterialXTextureBinding> textures;
  bool hasMaterialOpinion = false;
};

MaterialXParseResult ParseMaterialXNetwork(const HdMaterialNetwork2& network, const HydraMaterial& defaultMaterial);

void ApplyMaterialXTextureId(HydraMaterial& material, TextureUsage usage, int textureId);

PXR_NAMESPACE_CLOSE_SCOPE
