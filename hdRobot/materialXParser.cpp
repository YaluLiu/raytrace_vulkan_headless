#include "materialXParser.h"

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/usd/sdf/assetPath.h>

#include <algorithm>
#include <cmath>
#include <set>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
constexpr int kMaxMaterialXTraversalDepth = 32;

enum class ShaderFamily
{
  Unknown,
  UsdPreviewSurface,
  MaterialXSurface,
  MaterialXStandardSurface,
  MaterialXOpenPbrSurface,
  MaterialXOrenNayarDiffuseBsdf,
  MaterialXConductorBsdf,
  MaterialXDielectricBsdf,
  MaterialXUniformEdf,
};

enum class MaterialSemantic
{
  BaseColor,
  Metallic,
  Roughness,
  Normal,
  Emission,
  Opacity,
  Transmission,
  TransmissionColor,
  Subsurface,
  SubsurfaceColor,
  SubsurfaceScale,
  Ior,
  Unsupported,
};

enum class ValueKind
{
  Float,
  Color3,
  TextureOnly,
  FloatOrColor3,
};

struct MaterialResolvedInput
{
  bool hasValue = false;
  VtValue value;
  bool hasTexture = false;
  std::string texturePath;
  bool hasPrimvar = false;
  TfToken primvarName;
  TfToken outputName;
  TfToken channel;
  TfToken inputName;
};

struct MaterialInputRule
{
  ShaderFamily family;
  MaterialSemantic semantic;
  std::vector<TfToken> inputNames;
  TextureUsage textureUsage = TextureUsage::Unknown;
  ValueKind valueKind = ValueKind::Float;
  bool acceptsTexture = true;
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
  if (id == "UsdPreviewSurface")
  {
    return ShaderFamily::UsdPreviewSurface;
  }
  if (id == "ND_surface")
  {
    return ShaderFamily::MaterialXSurface;
  }
  if (id.find("open_pbr_surface") != std::string::npos)
  {
    return ShaderFamily::MaterialXOpenPbrSurface;
  }
  if (id == "ND_standard_surface_surfaceshader" || id.find("standard_surface") != std::string::npos)
  {
    return ShaderFamily::MaterialXStandardSurface;
  }
  if (id.find("oren_nayar_diffuse_bsdf") != std::string::npos)
  {
    return ShaderFamily::MaterialXOrenNayarDiffuseBsdf;
  }
  if (id.find("conductor_bsdf") != std::string::npos)
  {
    return ShaderFamily::MaterialXConductorBsdf;
  }
  if (id.find("dielectric_bsdf") != std::string::npos)
  {
    return ShaderFamily::MaterialXDielectricBsdf;
  }
  if (id.find("uniform_edf") != std::string::npos)
  {
    return ShaderFamily::MaterialXUniformEdf;
  }
  return ShaderFamily::Unknown;
}

bool IsTerminalSurfaceFamily(ShaderFamily family)
{
  return family == ShaderFamily::UsdPreviewSurface || family == ShaderFamily::MaterialXStandardSurface ||
         family == ShaderFamily::MaterialXOpenPbrSurface || family == ShaderFamily::MaterialXSurface;
}

bool IsMaterialXClosureFamily(ShaderFamily family)
{
  return family == ShaderFamily::MaterialXOrenNayarDiffuseBsdf || family == ShaderFamily::MaterialXConductorBsdf ||
         family == ShaderFamily::MaterialXDielectricBsdf || family == ShaderFamily::MaterialXUniformEdf;
}

bool IsParseableMaterialFamily(ShaderFamily family)
{
  return family == ShaderFamily::UsdPreviewSurface || family == ShaderFamily::MaterialXStandardSurface ||
         family == ShaderFamily::MaterialXOpenPbrSurface || IsMaterialXClosureFamily(family);
}

int CountKnownSurfaceInputs(const HdMaterialNode2& node, ShaderFamily family)
{
  std::vector<TfToken> inputNames = {
      TfToken("base_color"),         TfToken("metalness"),       TfToken("specular_roughness"),
      TfToken("normal"),             TfToken("emission"),        TfToken("emission_color"),
      TfToken("opacity"),            TfToken("transmission"),    TfToken("transmission_color"),
      TfToken("subsurface"),         TfToken("subsurface_color"), TfToken("subsurface_scale"),
  };
  if (family == ShaderFamily::MaterialXSurface)
  {
    inputNames = {TfToken("bsdf"), TfToken("edf"), TfToken("opacity"), TfToken("thin_walled")};
  }
  else if (family == ShaderFamily::UsdPreviewSurface)
  {
    inputNames = {TfToken("diffuseColor"), TfToken("metallic"), TfToken("roughness"), TfToken("normal"),
                  TfToken("emissiveColor"), TfToken("opacity"), TfToken("ior"), TfToken("occlusion")};
  }
  if (family == ShaderFamily::MaterialXOpenPbrSurface)
  {
    inputNames.push_back(TfToken("base_metalness"));
    inputNames.push_back(TfToken("base_diffuse_roughness"));
    inputNames.push_back(TfToken("geometry_normal"));
    inputNames.push_back(TfToken("emission_luminance"));
    inputNames.push_back(TfToken("base_weight"));
  }

  int score = 0;
  for (const TfToken& inputName : inputNames)
  {
    if (node.parameters.find(inputName) != node.parameters.end() ||
        node.inputConnections.find(inputName) != node.inputConnections.end())
    {
      ++score;
    }
  }
  return score;
}

bool IsSurfaceTerminalName(const TfToken& terminalName)
{
  const std::string name = terminalName.GetString();
  return name == "mtlx:surface" || name == "surface" || name.find("surface") != std::string::npos;
}

bool IsMtlxSurfaceTerminalName(const TfToken& terminalName)
{
  return terminalName.GetString() == "mtlx:surface";
}

bool IsBetterSurfaceCandidate(const HdMaterialNetwork2& network, const SurfaceShaderCandidate& candidate,
                              const SurfaceShaderCandidate& current)
{
  if (IsMtlxSurfaceTerminalName(candidate.terminalName) != IsMtlxSurfaceTerminalName(current.terminalName))
  {
    return IsMtlxSurfaceTerminalName(candidate.terminalName);
  }

  const auto candidateNodeIt = network.nodes.find(candidate.nodePath);
  const auto currentNodeIt = network.nodes.find(current.nodePath);
  const int candidateScore = candidateNodeIt == network.nodes.end()
                                 ? 0
                                 : CountKnownSurfaceInputs(candidateNodeIt->second, candidate.family);
  const int currentScore =
      currentNodeIt == network.nodes.end() ? 0 : CountKnownSurfaceInputs(currentNodeIt->second, current.family);
  if (candidateScore != currentScore)
  {
    return candidateScore > currentScore;
  }

  if (candidate.family != current.family)
  {
    if (candidate.family == ShaderFamily::MaterialXStandardSurface)
    {
      return true;
    }
    if (current.family == ShaderFamily::MaterialXStandardSurface)
    {
      return false;
    }
    return candidate.family == ShaderFamily::MaterialXSurface;
  }
  return candidate.nodePath.GetString() < current.nodePath.GetString();
}

std::vector<SurfaceShaderCandidate> CollectSurfaceShaderCandidates(const HdMaterialNetwork2& network)
{
  std::vector<SurfaceShaderCandidate> candidates;
  for (const auto& nodePair : network.nodes)
  {
    const ShaderFamily family = IdentifyShaderFamily(nodePair.second.nodeTypeId);
    if (!IsTerminalSurfaceFamily(family))
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

const std::vector<MaterialInputRule>& MaterialInputRules()
{
  static const std::vector<MaterialInputRule> rules = {
      {ShaderFamily::UsdPreviewSurface,
       MaterialSemantic::BaseColor,
       {TfToken("diffuseColor")},
       TextureUsage::BaseColor,
       ValueKind::Color3,
       true},
      {ShaderFamily::UsdPreviewSurface,
       MaterialSemantic::Metallic,
       {TfToken("metallic")},
       TextureUsage::Metallic,
       ValueKind::Float,
       true},
      {ShaderFamily::UsdPreviewSurface,
       MaterialSemantic::Roughness,
       {TfToken("roughness")},
       TextureUsage::Roughness,
       ValueKind::Float,
       true},
      {ShaderFamily::UsdPreviewSurface,
       MaterialSemantic::Normal,
       {TfToken("normal")},
       TextureUsage::Normal,
       ValueKind::TextureOnly,
       true},
      {ShaderFamily::UsdPreviewSurface,
       MaterialSemantic::Emission,
       {TfToken("emissiveColor")},
       TextureUsage::Emission,
       ValueKind::Color3,
       true},
      {ShaderFamily::UsdPreviewSurface,
       MaterialSemantic::Opacity,
       {TfToken("opacity")},
       TextureUsage::Opacity,
       ValueKind::Float,
       true},
      {ShaderFamily::MaterialXStandardSurface,
       MaterialSemantic::BaseColor,
       {TfToken("base_color"), TfToken("diffuseColor")},
       TextureUsage::BaseColor,
       ValueKind::Color3,
       true},
      {ShaderFamily::MaterialXStandardSurface,
       MaterialSemantic::Metallic,
       {TfToken("metalness")},
       TextureUsage::Metallic,
       ValueKind::Float,
       true},
      {ShaderFamily::MaterialXStandardSurface,
       MaterialSemantic::Roughness,
       {TfToken("specular_roughness")},
       TextureUsage::Roughness,
       ValueKind::Float,
       true},
      {ShaderFamily::MaterialXStandardSurface,
       MaterialSemantic::Normal,
       {TfToken("normal"), TfToken("normalmap")},
       TextureUsage::Normal,
       ValueKind::TextureOnly,
       true},
      {ShaderFamily::MaterialXStandardSurface,
       MaterialSemantic::Emission,
       {TfToken("emission"), TfToken("emission_color"), TfToken("emissiveColor")},
       TextureUsage::Emission,
       ValueKind::FloatOrColor3,
       true},
      {ShaderFamily::MaterialXStandardSurface,
       MaterialSemantic::Opacity,
       {TfToken("opacity"), TfToken("alpha")},
       TextureUsage::Opacity,
       ValueKind::Float,
       true},
      {ShaderFamily::MaterialXStandardSurface,
       MaterialSemantic::Transmission,
       {TfToken("transmission")},
       TextureUsage::Unknown,
       ValueKind::Float,
       false},
      {ShaderFamily::MaterialXStandardSurface,
       MaterialSemantic::TransmissionColor,
       {TfToken("transmission_color")},
       TextureUsage::Unknown,
       ValueKind::Color3,
       false},
      {ShaderFamily::MaterialXStandardSurface,
       MaterialSemantic::Subsurface,
       {TfToken("subsurface")},
       TextureUsage::Subsurface,
       ValueKind::Float,
       true},
      {ShaderFamily::MaterialXStandardSurface,
       MaterialSemantic::SubsurfaceColor,
       {TfToken("subsurface_color")},
       TextureUsage::Subsurface,
       ValueKind::Color3,
       true},
      {ShaderFamily::MaterialXStandardSurface,
       MaterialSemantic::SubsurfaceScale,
       {TfToken("subsurface_scale")},
       TextureUsage::Unknown,
       ValueKind::Float,
       false},
      {ShaderFamily::MaterialXOpenPbrSurface,
       MaterialSemantic::BaseColor,
       {TfToken("base_color")},
       TextureUsage::BaseColor,
       ValueKind::Color3,
       true},
      {ShaderFamily::MaterialXOpenPbrSurface,
       MaterialSemantic::Metallic,
       {TfToken("base_metalness"), TfToken("metalness")},
       TextureUsage::Metallic,
       ValueKind::Float,
       true},
      {ShaderFamily::MaterialXOpenPbrSurface,
       MaterialSemantic::Roughness,
       {TfToken("specular_roughness"), TfToken("base_diffuse_roughness")},
       TextureUsage::Roughness,
       ValueKind::Float,
       true},
      {ShaderFamily::MaterialXOpenPbrSurface,
       MaterialSemantic::Normal,
       {TfToken("geometry_normal"), TfToken("normal")},
       TextureUsage::Normal,
       ValueKind::TextureOnly,
       true},
      {ShaderFamily::MaterialXOpenPbrSurface,
       MaterialSemantic::Emission,
       {TfToken("emission_color"), TfToken("emission_luminance")},
       TextureUsage::Emission,
       ValueKind::FloatOrColor3,
       true},
      {ShaderFamily::MaterialXOpenPbrSurface,
       MaterialSemantic::Unsupported,
       {TfToken("base_weight")},
       TextureUsage::Unknown,
       ValueKind::Float,
       false},
      {ShaderFamily::MaterialXOpenPbrSurface,
       MaterialSemantic::Unsupported,
       {TfToken("coat_color"), TfToken("coat_roughness"), TfToken("fuzz_color"), TfToken("thin_film_thickness")},
       TextureUsage::Unknown,
       ValueKind::FloatOrColor3,
       true},
      {ShaderFamily::MaterialXOrenNayarDiffuseBsdf,
       MaterialSemantic::BaseColor,
       {TfToken("color")},
       TextureUsage::BaseColor,
       ValueKind::Color3,
       true},
      {ShaderFamily::MaterialXOrenNayarDiffuseBsdf,
       MaterialSemantic::Roughness,
       {TfToken("roughness")},
       TextureUsage::Roughness,
       ValueKind::Float,
       true},
      {ShaderFamily::MaterialXConductorBsdf,
       MaterialSemantic::Roughness,
       {TfToken("roughness")},
       TextureUsage::Roughness,
       ValueKind::Float,
       true},
      {ShaderFamily::MaterialXDielectricBsdf,
       MaterialSemantic::Roughness,
       {TfToken("roughness")},
       TextureUsage::Roughness,
       ValueKind::Float,
       true},
      {ShaderFamily::MaterialXDielectricBsdf,
       MaterialSemantic::TransmissionColor,
       {TfToken("color"), TfToken("tint")},
       TextureUsage::Unknown,
       ValueKind::Color3,
       false},
      {ShaderFamily::MaterialXDielectricBsdf,
       MaterialSemantic::Ior,
       {TfToken("ior")},
       TextureUsage::Unknown,
       ValueKind::Float,
       false},
      {ShaderFamily::MaterialXUniformEdf,
       MaterialSemantic::Emission,
       {TfToken("color")},
       TextureUsage::Emission,
       ValueKind::Color3,
       true},
  };
  return rules;
}

bool TryGetNodeSurfaceCandidate(const HdMaterialNetwork2& network, const SdfPath& nodePath, const TfToken& terminalName,
                                SurfaceShaderCandidate* candidate)
{
  const auto nodeIt = network.nodes.find(nodePath);
  if (nodeIt == network.nodes.end())
  {
    return false;
  }

  const ShaderFamily family = IdentifyShaderFamily(nodeIt->second.nodeTypeId);
  if (!IsTerminalSurfaceFamily(family))
  {
    return false;
  }

  candidate->nodePath = nodePath;
  candidate->family = family;
  candidate->terminalName = terminalName;
  return true;
}

bool SelectSurfaceShaderCandidate(const HdMaterialNetwork2& network, SurfaceShaderCandidate* selected)
{
  bool hasSelected = false;
  for (const auto& terminalPair : network.terminals)
  {
    const TfToken& terminalName = terminalPair.first;
    if (!IsSurfaceTerminalName(terminalName))
    {
      continue;
    }

    SurfaceShaderCandidate candidate;
    if (!TryGetNodeSurfaceCandidate(network, terminalPair.second.upstreamNode, terminalName, &candidate))
    {
      continue;
    }

    if (!hasSelected || IsBetterSurfaceCandidate(network, candidate, *selected))
    {
      *selected = candidate;
      hasSelected = true;
    }
  }

  if (hasSelected)
  {
    return true;
  }

  for (const SurfaceShaderCandidate& candidate : CollectSurfaceShaderCandidates(network))
  {
    if (!hasSelected || IsBetterSurfaceCandidate(network, candidate, *selected))
    {
      *selected = candidate;
      hasSelected = true;
    }
  }
  return hasSelected;
}

void CollectConnectedClosureNodes(const HdMaterialNetwork2& network, const SdfPath& nodePath, int depth,
                                  std::set<SdfPath>* visited, std::vector<SdfPath>* nodes)
{
  if (depth > kMaxMaterialXTraversalDepth || visited->find(nodePath) != visited->end())
  {
    return;
  }
  visited->insert(nodePath);

  const auto nodeIt = network.nodes.find(nodePath);
  if (nodeIt == network.nodes.end())
  {
    return;
  }

  const ShaderFamily family = IdentifyShaderFamily(nodeIt->second.nodeTypeId);
  if (IsParseableMaterialFamily(family) && family != ShaderFamily::MaterialXSurface)
  {
    nodes->push_back(nodePath);
    return;
  }

  for (const auto& connPair : nodeIt->second.inputConnections)
  {
    for (const HdMaterialConnection2& connection : connPair.second)
    {
      CollectConnectedClosureNodes(network, connection.upstreamNode, depth + 1, visited, nodes);
    }
  }
}

std::vector<SdfPath> GetMaterialXSurfaceNodesToParse(const HdMaterialNetwork2& network, const SdfPath& surfaceNodePath)
{
  const auto nodeIt = network.nodes.find(surfaceNodePath);
  if (nodeIt == network.nodes.end())
  {
    return {};
  }

  std::vector<SdfPath> nodes;
  std::set<SdfPath> visited;
  const std::vector<TfToken> closureInputs = {TfToken("bsdf"), TfToken("edf")};
  for (const TfToken& closureInput : closureInputs)
  {
    const auto connIt = nodeIt->second.inputConnections.find(closureInput);
    if (connIt == nodeIt->second.inputConnections.end())
    {
      continue;
    }
    for (const HdMaterialConnection2& connection : connIt->second)
    {
      CollectConnectedClosureNodes(network, connection.upstreamNode, 0, &visited, &nodes);
    }
  }
  return nodes;
}

std::vector<SdfPath> GetMaterialNodesToParse(const HdMaterialNetwork2& network)
{
  SurfaceShaderCandidate selected;
  if (SelectSurfaceShaderCandidate(network, &selected))
  {
    if (selected.family == ShaderFamily::MaterialXSurface)
    {
      const std::vector<SdfPath> closureNodes = GetMaterialXSurfaceNodesToParse(network, selected.nodePath);
      if (!closureNodes.empty())
      {
        return closureNodes;
      }
    }
    return {selected.nodePath};
  }

  std::vector<SdfPath> allNodes;
  for (const auto& nodePair : network.nodes)
  {
    allNodes.push_back(nodePair.first);
  }
  return allNodes;
}

bool IsFileParameter(const TfToken& name)
{
  return name == "file" || name == "filename";
}

bool IsPrimvarReaderNode(const HdMaterialNode2& node)
{
  const std::string id = node.nodeTypeId.GetString();
  return id.find("UsdPrimvarReader") != std::string::npos || id.find("geompropvalue") != std::string::npos;
}

bool IsPrimvarNameParameter(const TfToken& name)
{
  return name == "varname" || name == "geomprop";
}

TfToken PrimvarNameFromValue(const VtValue& value)
{
  if (value.IsHolding<TfToken>())
  {
    return value.Get<TfToken>();
  }
  if (value.IsHolding<std::string>())
  {
    return TfToken(value.Get<std::string>());
  }
  return TfToken();
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

bool IsChannelOutputName(const TfToken& outputName)
{
  const std::string name = outputName.GetString();
  return name == "r" || name == "g" || name == "b" || name == "a" || name == "x" || name == "y" || name == "z" ||
         name == "w";
}

void SetResolvedOutputMetadata(const TfToken& outputName, MaterialResolvedInput* resolved)
{
  if (outputName.IsEmpty())
  {
    return;
  }
  resolved->outputName = outputName;
  if (IsChannelOutputName(outputName))
  {
    resolved->channel = outputName;
  }
}

bool ResolveUpstreamTexture(const HdMaterialNetwork2& network, const SdfPath& nodePath, const TfToken& outputName,
                            int depth, std::set<SdfPath>* visited, MaterialResolvedInput* resolved)
{
  if (depth > kMaxMaterialXTraversalDepth || visited->find(nodePath) != visited->end())
  {
    return false;
  }
  visited->insert(nodePath);

  const auto nodeIt = network.nodes.find(nodePath);
  if (nodeIt == network.nodes.end())
  {
    return false;
  }

  const HdMaterialNode2& node = nodeIt->second;
  for (const auto& paramPair : node.parameters)
  {
    if (IsFileParameter(paramPair.first))
    {
      std::string texturePath = TexturePathFromValue(paramPair.second);
      if (!texturePath.empty())
      {
        resolved->hasTexture = true;
        resolved->texturePath = texturePath;
        SetResolvedOutputMetadata(outputName, resolved);
        return true;
      }
    }
  }

  for (const auto& connPair : node.inputConnections)
  {
    for (const HdMaterialConnection2& connection : connPair.second)
    {
      if (ResolveUpstreamTexture(network, connection.upstreamNode, connection.upstreamOutputName, depth + 1, visited,
                                 resolved))
      {
        if (resolved->outputName.IsEmpty())
        {
          SetResolvedOutputMetadata(outputName, resolved);
        }
        return true;
      }
    }
  }

  return false;
}

bool ResolveUpstreamPrimvar(const HdMaterialNetwork2& network, const SdfPath& nodePath, int depth,
                            std::set<SdfPath>* visited, MaterialResolvedInput* resolved)
{
  if (depth > kMaxMaterialXTraversalDepth || visited->find(nodePath) != visited->end())
  {
    return false;
  }
  visited->insert(nodePath);

  const auto nodeIt = network.nodes.find(nodePath);
  if (nodeIt == network.nodes.end())
  {
    return false;
  }

  const HdMaterialNode2& node = nodeIt->second;
  if (IsPrimvarReaderNode(node))
  {
    for (const auto& paramPair : node.parameters)
    {
      if (IsPrimvarNameParameter(paramPair.first))
      {
        const TfToken primvarName = PrimvarNameFromValue(paramPair.second);
        if (!primvarName.IsEmpty())
        {
          resolved->hasPrimvar = true;
          resolved->primvarName = primvarName;
          return true;
        }
      }
    }
  }

  for (const auto& connPair : node.inputConnections)
  {
    for (const HdMaterialConnection2& connection : connPair.second)
    {
      if (ResolveUpstreamPrimvar(network, connection.upstreamNode, depth + 1, visited, resolved))
      {
        return true;
      }
    }
  }

  return false;
}

bool FindInputTexture(const HdMaterialNetwork2& network, const HdMaterialNode2& node, const TfToken& inputName,
                      MaterialResolvedInput* resolved)
{
  const auto connectionsIt = node.inputConnections.find(inputName);
  if (connectionsIt == node.inputConnections.end())
  {
    return false;
  }

  for (const HdMaterialConnection2& connection : connectionsIt->second)
  {
    std::set<SdfPath> visited;
    if (ResolveUpstreamTexture(network, connection.upstreamNode, connection.upstreamOutputName, 0, &visited, resolved))
    {
      return true;
    }
  }
  return false;
}

bool FindInputPrimvar(const HdMaterialNetwork2& network, const HdMaterialNode2& node, const TfToken& inputName,
                      MaterialResolvedInput* resolved)
{
  const auto connectionsIt = node.inputConnections.find(inputName);
  if (connectionsIt == node.inputConnections.end())
  {
    return false;
  }

  for (const HdMaterialConnection2& connection : connectionsIt->second)
  {
    std::set<SdfPath> visited;
    if (ResolveUpstreamPrimvar(network, connection.upstreamNode, 0, &visited, resolved))
    {
      return true;
    }
  }
  return false;
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
  if (value.IsHolding<GfVec2f>())
  {
    const GfVec2f vec = value.Get<GfVec2f>();
    *result = vec[0];
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
  float scalar = 0.0f;
  if (ReadFloat(value, &scalar))
  {
    *result = glm::vec3(scalar);
    return true;
  }
  return false;
}

bool ReadParameterFloat(const HdMaterialNode2& node, const TfToken& name, float* result)
{
  const auto paramIt = node.parameters.find(name);
  if (paramIt == node.parameters.end())
  {
    return false;
  }
  return ReadFloat(paramIt->second, result);
}

bool ReadParameterVec3(const HdMaterialNode2& node, const TfToken& name, glm::vec3* result)
{
  const auto paramIt = node.parameters.find(name);
  if (paramIt == node.parameters.end())
  {
    return false;
  }
  return ReadVec3(paramIt->second, result);
}

glm::vec3 ClampColor(const glm::vec3& value)
{
  return glm::vec3(std::clamp(value.x, 0.0f, 1.0f), std::clamp(value.y, 0.0f, 1.0f),
                   std::clamp(value.z, 0.0f, 1.0f));
}

glm::vec3 ComputeConductorF0(const glm::vec3& ior, const glm::vec3& extinction)
{
  glm::vec3 result(0.9f);
  for (int i = 0; i < 3; ++i)
  {
    const float n = std::max(ior[i], 0.0f);
    const float k = std::max(extinction[i], 0.0f);
    const float numerator = (n - 1.0f) * (n - 1.0f) + k * k;
    const float denominator = (n + 1.0f) * (n + 1.0f) + k * k;
    result[i] = denominator > 0.0f ? numerator / denominator : 0.9f;
  }
  return ClampColor(result);
}

void ApplyMaterialXClosureDefaults(MaterialXParseResult& result, const HdMaterialNode2& node, ShaderFamily family)
{
  HydraMaterial& material = result.material;

  switch (family)
  {
    case ShaderFamily::MaterialXOrenNayarDiffuseBsdf:
      material.metallicFactor = 0.0f;
      result.hasMaterialOpinion = true;
      break;
    case ShaderFamily::MaterialXConductorBsdf:
    {
      material.metallicFactor = 1.0f;
      material.roughnessFactor = 0.1f;

      glm::vec3 ior;
      glm::vec3 extinction;
      if (ReadParameterVec3(node, TfToken("ior"), &ior) && ReadParameterVec3(node, TfToken("extinction"), &extinction))
      {
        material.baseColorFactor = ComputeConductorF0(ior, extinction);
        material.diffuse = material.baseColorFactor;
        material.specular = material.baseColorFactor;
      }

      float roughness = 0.0f;
      if (ReadParameterFloat(node, TfToken("roughness"), &roughness))
      {
        material.roughnessFactor = std::clamp(roughness, 0.02f, 1.0f);
      }
      result.hasMaterialOpinion = true;
      break;
    }
    case ShaderFamily::MaterialXDielectricBsdf:
    {
      material.metallicFactor = 0.0f;
      material.transmissionFactor = 1.0f;
      material.transmissionColorFactor = glm::vec3(1.0f);
      material.baseColorFactor = glm::vec3(1.0f);
      material.diffuse = material.baseColorFactor;

      float ior = 0.0f;
      if (ReadParameterFloat(node, TfToken("ior"), &ior))
      {
        material.ior = ior;
      }
      float roughness = 0.0f;
      if (ReadParameterFloat(node, TfToken("roughness"), &roughness))
      {
        material.roughnessFactor = std::clamp(roughness, 0.02f, 1.0f);
      }
      result.hasMaterialOpinion = true;
      break;
    }
    case ShaderFamily::MaterialXUniformEdf:
    {
      glm::vec3 emission;
      if (ReadParameterVec3(node, TfToken("color"), &emission))
      {
        material.emissionFactor = emission;
        material.emission = emission;
        result.hasMaterialOpinion = true;
      }
      break;
    }
    default:
      break;
  }
}

MaterialXTextureBinding MakeTextureBinding(const MaterialResolvedInput& resolved, TextureUsage usage)
{
  MaterialXTextureBinding binding;
  binding.assetPath = resolved.texturePath;
  binding.usage = usage;
  binding.sourceOutput = resolved.outputName;
  binding.channel = resolved.channel;
  binding.inputName = resolved.inputName;
  return binding;
}

void AddTextureBinding(MaterialXParseResult& result, const MaterialResolvedInput& resolved, TextureUsage usage)
{
  if (resolved.texturePath.empty())
  {
    return;
  }
  result.textures.push_back(MakeTextureBinding(resolved, usage));
  result.hasMaterialOpinion = true;
}

void AddUnsupportedTextureBinding(MaterialXParseResult& result, const MaterialResolvedInput& resolved)
{
  if (resolved.texturePath.empty())
  {
    return;
  }
  result.unsupportedTextures.push_back(MakeTextureBinding(resolved, TextureUsage::Unknown));
}

void ApplyTextureOnlyFactorDefault(HydraMaterial& material, MaterialSemantic semantic)
{
  switch (semantic)
  {
    case MaterialSemantic::BaseColor:
      material.baseColorFactor = glm::vec3(1.0f);
      material.diffuse = material.baseColorFactor;
      break;
    case MaterialSemantic::Emission:
      material.emissionFactor = glm::vec3(1.0f);
      material.emission = material.emissionFactor;
      break;
    default:
      break;
  }
}

bool ResolveInput(const HdMaterialNetwork2& network, const HdMaterialNode2& node, const MaterialInputRule& rule,
                  MaterialResolvedInput* resolved)
{
  for (const TfToken& inputName : rule.inputNames)
  {
    const auto paramIt = node.parameters.find(inputName);
    if (paramIt != node.parameters.end())
    {
      resolved->hasValue = true;
      resolved->value = paramIt->second;
      resolved->inputName = inputName;
    }

    if (rule.acceptsTexture)
    {
      FindInputTexture(network, node, inputName, resolved);
      if (resolved->hasTexture)
      {
        resolved->inputName = inputName;
      }
      if (!resolved->hasTexture && FindInputPrimvar(network, node, inputName, resolved))
      {
        resolved->inputName = inputName;
      }
    }

    if (resolved->hasValue || resolved->hasTexture || resolved->hasPrimvar)
    {
      return true;
    }
  }
  return false;
}

void ApplyResolvedValue(HydraMaterial& material, const MaterialInputRule& rule, const VtValue& value,
                        bool* hasMaterialOpinion)
{
  glm::vec3 color;
  switch (rule.semantic)
  {
    case MaterialSemantic::BaseColor:
      if (ReadVec3(value, &color))
      {
        material.baseColorFactor = color;
        material.diffuse = color;
        *hasMaterialOpinion = true;
      }
      break;
    case MaterialSemantic::Metallic:
      if (ReadFloat(value, &material.metallicFactor))
      {
        *hasMaterialOpinion = true;
      }
      break;
    case MaterialSemantic::Roughness:
      if (ReadFloat(value, &material.roughnessFactor))
      {
        *hasMaterialOpinion = true;
      }
      break;
    case MaterialSemantic::Emission:
      if (ReadVec3(value, &color))
      {
        material.emissionFactor = color;
        material.emission = color;
        *hasMaterialOpinion = true;
      }
      break;
    case MaterialSemantic::Opacity:
      if (ReadFloat(value, &material.opacityFactor))
      {
        *hasMaterialOpinion = true;
      }
      break;
    case MaterialSemantic::Transmission:
      if (ReadFloat(value, &material.transmissionFactor))
      {
        *hasMaterialOpinion = true;
      }
      break;
    case MaterialSemantic::TransmissionColor:
      if (ReadVec3(value, &material.transmissionColorFactor))
      {
        *hasMaterialOpinion = true;
      }
      break;
    case MaterialSemantic::Subsurface:
      if (ReadFloat(value, &material.subsurfaceFactor))
      {
        *hasMaterialOpinion = true;
      }
      break;
    case MaterialSemantic::SubsurfaceColor:
      if (ReadVec3(value, &material.subsurfaceColorFactor))
      {
        *hasMaterialOpinion = true;
      }
      break;
    case MaterialSemantic::SubsurfaceScale:
      if (ReadFloat(value, &material.subsurfaceScale))
      {
        *hasMaterialOpinion = true;
      }
      break;
    case MaterialSemantic::Ior:
      if (ReadFloat(value, &material.ior))
      {
        *hasMaterialOpinion = true;
      }
      break;
    case MaterialSemantic::Normal:
    case MaterialSemantic::Unsupported:
      break;
  }
}

void ApplyResolvedInput(MaterialXParseResult& result, const MaterialInputRule& rule,
                        const MaterialResolvedInput& resolved)
{
  if (resolved.hasValue)
  {
    ApplyResolvedValue(result.material, rule, resolved.value, &result.hasMaterialOpinion);
  }
  if (resolved.hasTexture)
  {
    if (rule.semantic == MaterialSemantic::Unsupported || rule.textureUsage == TextureUsage::Unknown)
    {
      AddUnsupportedTextureBinding(result, resolved);
      return;
    }

    AddTextureBinding(result, resolved, rule.textureUsage);
    if (!resolved.hasValue)
    {
      ApplyTextureOnlyFactorDefault(result.material, rule.semantic);
    }
    if (rule.semantic == MaterialSemantic::BaseColor)
    {
      result.material.texturePath = resolved.texturePath;
    }
  }
  if (resolved.hasPrimvar && rule.semantic == MaterialSemantic::BaseColor)
  {
    result.baseColorPrimvarName = resolved.primvarName;
    result.hasMaterialOpinion = true;
  }
}
}  // namespace

void ApplyMaterialXTextureId(HydraMaterial& material, TextureUsage usage, int textureId)
{
  switch (usage)
  {
    case TextureUsage::BaseColor:
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

  // This parser targets UsdPreviewSurface, MaterialX standard_surface/OpenPBR,
  // and a small set of MaterialX BSDF/EDF closures used by asset-library scenes.
  // Unknown shader families are skipped conservatively.
  for (const SdfPath& nodePath : GetMaterialNodesToParse(network))
  {
    const auto nodeIt = network.nodes.find(nodePath);
    if (nodeIt == network.nodes.end())
    {
      continue;
    }
    const HdMaterialNode2& node = nodeIt->second;
    ShaderFamily family = IdentifyShaderFamily(node.nodeTypeId);
    if (family == ShaderFamily::Unknown)
    {
      continue;
    }

    ApplyMaterialXClosureDefaults(result, node, family);

    for (const MaterialInputRule& rule : MaterialInputRules())
    {
      if (rule.family != family)
      {
        continue;
      }

      MaterialResolvedInput resolved;
      if (ResolveInput(network, node, rule, &resolved))
      {
        ApplyResolvedInput(result, rule, resolved);
      }
    }
  }

  return result;
}

PXR_NAMESPACE_CLOSE_SCOPE
