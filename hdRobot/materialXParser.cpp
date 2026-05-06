#include "materialXParser.h"

#include <pxr/base/gf/vec4f.h>
#include <pxr/usd/sdf/assetPath.h>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
enum class ShaderFamily
{
  Unknown,
  MaterialXStandardSurface,
  MaterialXOpenPbrSurface,
};

struct SurfaceShaderCandidate
{
  SdfPath nodePath;
  ShaderFamily family = ShaderFamily::Unknown;
  TfToken terminalName;
  TfToken renderContext;
};

ShaderFamily IdentifyShaderFamily(const TfToken& shaderId)
{
  const std::string id = shaderId.GetString();
  if (id.find("open_pbr_surface") != std::string::npos)
  {
    return ShaderFamily::MaterialXOpenPbrSurface;
  }
  if (id == "ND_standard_surface_surfaceshader" || id.find("standard_surface") != std::string::npos)
  {
    return ShaderFamily::MaterialXStandardSurface;
  }
  return ShaderFamily::Unknown;
}

bool IsMaterialXSurfaceFamily(ShaderFamily family)
{
  return family == ShaderFamily::MaterialXStandardSurface || family == ShaderFamily::MaterialXOpenPbrSurface;
}

std::vector<SurfaceShaderCandidate> CollectSurfaceShaderCandidates(const HdMaterialNetwork2& network)
{
  std::vector<SurfaceShaderCandidate> candidates;
  for (const auto& nodePair : network.nodes)
  {
    const ShaderFamily family = IdentifyShaderFamily(nodePair.second.nodeTypeId);
    if (!IsMaterialXSurfaceFamily(family))
    {
      continue;
    }

    SurfaceShaderCandidate candidate;
    candidate.nodePath = nodePair.first;
    candidate.family = family;
    candidates.push_back(candidate);
  }
  return candidates;
}

bool IsFileParameter(const TfToken& name)
{
  return name == "file" || name == "filename";
}

std::string AssetPathString(const SdfAssetPath& assetPath)
{
  std::string texturePath = assetPath.GetResolvedPath();
  if (texturePath.empty())
  {
    texturePath = assetPath.GetAssetPath();
  }
  return texturePath;
}

std::string TexturePathFromValue(const VtValue& value)
{
  if (value.IsHolding<SdfAssetPath>())
  {
    return AssetPathString(value.Get<SdfAssetPath>());
  }
  if (value.IsHolding<std::string>())
  {
    return value.Get<std::string>();
  }
  if (value.IsHolding<TfToken>())
  {
    return value.Get<TfToken>().GetString();
  }
  return {};
}

std::string FindUpstreamTexturePath(const HdMaterialNetwork2& network, const SdfPath& nodePath, int depth = 0)
{
  if (depth > 8)
  {
    return {};
  }

  const auto nodeIt = network.nodes.find(nodePath);
  if (nodeIt == network.nodes.end())
  {
    return {};
  }

  const HdMaterialNode2& node = nodeIt->second;
  for (const auto& paramPair : node.parameters)
  {
    if (IsFileParameter(paramPair.first))
    {
      std::string texturePath = TexturePathFromValue(paramPair.second);
      if (!texturePath.empty())
      {
        return texturePath;
      }
    }
  }

  for (const auto& connPair : node.inputConnections)
  {
    for (const HdMaterialConnection2& connection : connPair.second)
    {
      std::string texturePath = FindUpstreamTexturePath(network, connection.upstreamNode, depth + 1);
      if (!texturePath.empty())
      {
        return texturePath;
      }
    }
  }

  return {};
}

std::string FindInputTexturePath(const HdMaterialNetwork2& network, const HdMaterialNode2& node,
                                 const TfToken& inputName)
{
  const auto connectionsIt = node.inputConnections.find(inputName);
  if (connectionsIt == node.inputConnections.end())
  {
    return {};
  }

  for (const HdMaterialConnection2& connection : connectionsIt->second)
  {
    std::string texturePath = FindUpstreamTexturePath(network, connection.upstreamNode);
    if (!texturePath.empty())
    {
      return texturePath;
    }
  }
  return {};
}

bool ReadFloat(const VtValue& value, float* result)
{
  if (result == nullptr)
  {
    return false;
  }
  if (value.IsHolding<float>())
  {
    *result = value.Get<float>();
    return true;
  }
  if (value.IsHolding<double>())
  {
    *result = static_cast<float>(value.Get<double>());
    return true;
  }
  if (value.IsHolding<int>())
  {
    *result = static_cast<float>(value.Get<int>());
    return true;
  }
  return false;
}

bool ReadVec3(const VtValue& value, glm::vec3* result)
{
  if (result == nullptr)
  {
    return false;
  }
  if (value.IsHolding<GfVec3f>())
  {
    const GfVec3f vec = value.Get<GfVec3f>();
    *result = glm::vec3(vec[0], vec[1], vec[2]);
    return true;
  }
  if (value.IsHolding<GfVec4f>())
  {
    const GfVec4f vec = value.Get<GfVec4f>();
    *result = glm::vec3(vec[0], vec[1], vec[2]);
    return true;
  }
  return false;
}

void AddTextureBinding(MaterialXParseResult& result, const std::string& texturePath, TextureUsage usage)
{
  if (texturePath.empty())
  {
    return;
  }
  result.textures.push_back({texturePath, usage});
  result.hasMaterialOpinion = true;
}
}  // namespace

void ApplyMaterialXTextureId(HydraMaterial& material, TextureUsage usage, int textureId)
{
  switch (usage)
  {
    case TextureUsage::BaseColor:
      material.textureID = textureId;
      material.baseColorTextureId = textureId;
      break;
    case TextureUsage::Metallic:
      material.metallicTextureId = textureId;
      break;
    case TextureUsage::Roughness:
      material.roughnessTextureId = textureId;
      break;
    case TextureUsage::Normal:
      material.normalTextureId = textureId;
      break;
    case TextureUsage::Emission:
      material.emissionTextureId = textureId;
      break;
    case TextureUsage::Opacity:
      material.opacityTextureId = textureId;
      break;
    case TextureUsage::Subsurface:
      material.subsurfaceTextureId = textureId;
      material.subsurfaceFactor = 1.0f;
      break;
    default:
      break;
  }
}

MaterialXParseResult ParseMaterialXNetwork(const HdMaterialNetwork2& network, const HydraMaterial& defaultMaterial)
{
  MaterialXParseResult result;
  result.material = defaultMaterial;

  // This parser intentionally targets MaterialX standard_surface and OpenPBR
  // surface shaders. Unknown shader families are skipped conservatively.
  const std::vector<SurfaceShaderCandidate> surfaceCandidates = CollectSurfaceShaderCandidates(network);
  (void)surfaceCandidates;

  for (const auto& nodePair : network.nodes)
  {
    const HdMaterialNode2& node = nodePair.second;
    for (const auto& connPair : node.inputConnections)
    {
      const TfToken& inputName = connPair.first;
      if (inputName == "diffuseColor" || inputName == "base_color")
      {
        const std::string texturePath = FindInputTexturePath(network, node, inputName);
        AddTextureBinding(result, texturePath, TextureUsage::BaseColor);
        if (!texturePath.empty())
        {
          result.material.texturePath = texturePath;
        }
      }
      else if (inputName == "metalness")
      {
        AddTextureBinding(result, FindInputTexturePath(network, node, inputName), TextureUsage::Metallic);
      }
      else if (inputName == "specular_roughness")
      {
        AddTextureBinding(result, FindInputTexturePath(network, node, inputName), TextureUsage::Roughness);
      }
      else if (inputName == "normal")
      {
        AddTextureBinding(result, FindInputTexturePath(network, node, inputName), TextureUsage::Normal);
      }
      else if (inputName == "emission" || inputName == "emissiveColor")
      {
        AddTextureBinding(result, FindInputTexturePath(network, node, inputName), TextureUsage::Emission);
      }
      else if (inputName == "opacity")
      {
        AddTextureBinding(result, FindInputTexturePath(network, node, inputName), TextureUsage::Opacity);
      }
      else if (inputName == "transmission")
      {
        float transmission = 0.0f;
        const auto paramIt = node.parameters.find(inputName);
        if (paramIt != node.parameters.end() && ReadFloat(paramIt->second, &transmission))
        {
          result.material.transmissionFactor = transmission;
          result.hasMaterialOpinion = true;
        }
      }
      else if (inputName == "subsurface")
      {
        AddTextureBinding(result, FindInputTexturePath(network, node, inputName), TextureUsage::Subsurface);
      }
    }

    for (const auto& paramPair : node.parameters)
    {
      const TfToken& paramName = paramPair.first;
      const VtValue& paramValue = paramPair.second;
      if (paramName == "diffuseColor")
      {
        glm::vec3 diffuse_color;
        if (ReadVec3(paramValue, &diffuse_color))
        {
          result.material.diffuse = diffuse_color;
          result.material.baseColorFactor = diffuse_color;
          result.hasMaterialOpinion = true;
        }
      }
      else if (paramName == "base_color")
      {
        glm::vec3 baseColor;
        if (ReadVec3(paramValue, &baseColor))
        {
          result.material.baseColorFactor = baseColor;
          result.material.diffuse = baseColor;
          result.hasMaterialOpinion = true;
        }
      }
      else if (paramName == "metalness")
      {
        if (ReadFloat(paramValue, &result.material.metallicFactor))
        {
          result.hasMaterialOpinion = true;
        }
      }
      else if (paramName == "specular_roughness")
      {
        if (ReadFloat(paramValue, &result.material.roughnessFactor))
        {
          result.hasMaterialOpinion = true;
        }
      }
      else if (paramName == "emissiveColor")
      {
        glm::vec3 emission;
        if (ReadVec3(paramValue, &emission))
        {
          result.material.emission = emission;
          result.material.emissionFactor = emission;
          result.hasMaterialOpinion = true;
        }
      }
      else if (paramName == "emission")
      {
        glm::vec3 emission;
        float emissionScale = 0.0f;
        if (ReadVec3(paramValue, &emission))
        {
          result.material.emissionFactor = emission;
          result.material.emission = emission;
          result.hasMaterialOpinion = true;
        }
        else if (ReadFloat(paramValue, &emissionScale))
        {
          result.material.emissionFactor = glm::vec3(emissionScale);
          result.material.emission = glm::vec3(emissionScale);
          result.hasMaterialOpinion = true;
        }
      }
      else if (paramName == "opacity")
      {
        if (ReadFloat(paramValue, &result.material.opacityFactor))
        {
          result.material.dissolve = result.material.opacityFactor;
          result.hasMaterialOpinion = true;
        }
      }
      else if (paramName == "transmission")
      {
        if (ReadFloat(paramValue, &result.material.transmissionFactor))
        {
          result.hasMaterialOpinion = true;
        }
      }
      else if (paramName == "transmission_color")
      {
        glm::vec3 transmissionColor;
        if (ReadVec3(paramValue, &transmissionColor))
        {
          result.material.transmissionColorFactor = transmissionColor;
          result.hasMaterialOpinion = true;
        }
      }
      else if (paramName == "subsurface")
      {
        if (ReadFloat(paramValue, &result.material.subsurfaceFactor))
        {
          result.hasMaterialOpinion = true;
        }
      }
      else if (paramName == "subsurface_color")
      {
        glm::vec3 subsurfaceColor;
        if (ReadVec3(paramValue, &subsurfaceColor))
        {
          result.material.subsurfaceColorFactor = subsurfaceColor;
          result.hasMaterialOpinion = true;
        }
      }
      else if (paramName == "subsurface_scale")
      {
        if (ReadFloat(paramValue, &result.material.subsurfaceScale))
        {
          result.hasMaterialOpinion = true;
        }
      }
    }
  }

  return result;
}

PXR_NAMESPACE_CLOSE_SCOPE
