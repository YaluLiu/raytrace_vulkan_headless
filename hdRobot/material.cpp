#include "material.h"

#include "pxr/imaging/hd/material.h"
#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/usd/sdf/assetPath.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
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

int RegisterTexture(HdRobotRenderParam& scene, const std::string& texturePath, TextureUsage usage)
{
  if (texturePath.empty())
  {
    return -1;
  }
  return scene.RegisterTexturePath(texturePath, usage);
}
}  // namespace

HdRobotMaterial::HdRobotMaterial(const SdfPath& id, HdRobotRenderParam& scene) : HdMaterial(id), _scene(scene)
{
  std::lock_guard guard(_scene.mutex);
  _mat_id = _scene.v_mat.size();
  _scene.v_mat.emplace_back(HydraMaterial());
}

void HdRobotMaterial::Finalize(HdRenderParam* renderParam)
{
  _scene.v_mat[_mat_id].set_default();
  _scene.MarkMaterialDirty(_mat_id);
}

HdDirtyBits HdRobotMaterial::GetInitialDirtyBitsMask() const
{
  return DirtyBits::DirtyParams;
}

void HdRobotMaterial::Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits)
{
  if (!TF_VERIFY(sceneDelegate)) return;
  bool pullMaterial = (*dirtyBits & DirtyBits::DirtyParams);

  *dirtyBits = DirtyBits::Clean;

  if (!pullMaterial)
  {
    return;
  }

  _scene.v_mat[_mat_id].set_default();
  const SdfPath& id = GetId();
  const VtValue& resource = sceneDelegate->GetMaterialResource(id);

  if (!resource.IsHolding<HdMaterialNetworkMap>())
  {
    return;
  }

  const HdMaterialNetworkMap& networkMap = resource.UncheckedGet<HdMaterialNetworkMap>();
  bool isVolume = false;

  HdMaterialNetwork2 network = HdConvertToHdMaterialNetwork2(networkMap, &isVolume);
  if (isVolume)
  {
    TF_WARN("Volume %s unsupported", id.GetText());
    return;
  }

  for (const auto& nodePair : network.nodes)
  {
    const HdMaterialNode2& node = nodePair.second;
    for (const auto& connPair : node.inputConnections)
    {
      const TfToken& inputName = connPair.first;
      HydraMaterial& material = _scene.v_mat[_mat_id];
      if (inputName == "diffuseColor" || inputName == "base_color")
      {
        const std::string texturePath = FindInputTexturePath(network, node, inputName);
        const int textureId = RegisterTexture(_scene, texturePath, TextureUsage::BaseColor);
        if (textureId >= 0)
        {
          material.texturePath = texturePath;
          material.textureID = textureId;
          material.baseColorTextureId = textureId;
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
      else if (inputName == "metalness")
      {
        const int textureId =
            RegisterTexture(_scene, FindInputTexturePath(network, node, inputName), TextureUsage::Metallic);
        if (textureId >= 0)
        {
          material.metallicTextureId = textureId;
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
      else if (inputName == "specular_roughness")
      {
        const int textureId =
            RegisterTexture(_scene, FindInputTexturePath(network, node, inputName), TextureUsage::Roughness);
        if (textureId >= 0)
        {
          material.roughnessTextureId = textureId;
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
      else if (inputName == "normal")
      {
        const int textureId =
            RegisterTexture(_scene, FindInputTexturePath(network, node, inputName), TextureUsage::Normal);
        if (textureId >= 0)
        {
          material.normalTextureId = textureId;
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
      else if (inputName == "emission" || inputName == "emissiveColor")
      {
        const int textureId =
            RegisterTexture(_scene, FindInputTexturePath(network, node, inputName), TextureUsage::Emission);
        if (textureId >= 0)
        {
          material.emissionTextureId = textureId;
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
      else if (inputName == "opacity")
      {
        const int textureId =
            RegisterTexture(_scene, FindInputTexturePath(network, node, inputName), TextureUsage::Opacity);
        if (textureId >= 0)
        {
          material.opacityTextureId = textureId;
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
      else if (inputName == "transmission")
      {
        float transmission = 0.0f;
        const auto paramIt = node.parameters.find(inputName);
        if (paramIt != node.parameters.end() && ReadFloat(paramIt->second, &transmission))
        {
          material.transmissionFactor = transmission;
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
      else if (inputName == "subsurface")
      {
        const int textureId =
            RegisterTexture(_scene, FindInputTexturePath(network, node, inputName), TextureUsage::Subsurface);
        if (textureId >= 0)
        {
          material.subsurfaceTextureId = textureId;
          material.subsurfaceFactor = 1.0f;
          _scene.MarkMaterialDirty(_mat_id);
        }
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
          _scene.v_mat[_mat_id].diffuse = diffuse_color;
          _scene.v_mat[_mat_id].baseColorFactor = diffuse_color;
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
      else if (paramName == "base_color")
      {
        glm::vec3 baseColor;
        if (ReadVec3(paramValue, &baseColor))
        {
          _scene.v_mat[_mat_id].baseColorFactor = baseColor;
          _scene.v_mat[_mat_id].diffuse = baseColor;
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
      else if (paramName == "metalness")
      {
        if (ReadFloat(paramValue, &_scene.v_mat[_mat_id].metallicFactor))
        {
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
      else if (paramName == "specular_roughness")
      {
        if (ReadFloat(paramValue, &_scene.v_mat[_mat_id].roughnessFactor))
        {
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
      else if (paramName == "emissiveColor")
      {
        glm::vec3 emission;
        if (ReadVec3(paramValue, &emission))
        {
          _scene.v_mat[_mat_id].emission = emission;
          _scene.v_mat[_mat_id].emissionFactor = emission;
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
      else if (paramName == "emission")
      {
        glm::vec3 emission;
        float emissionScale = 0.0f;
        if (ReadVec3(paramValue, &emission))
        {
          _scene.v_mat[_mat_id].emissionFactor = emission;
          _scene.v_mat[_mat_id].emission = emission;
          _scene.MarkMaterialDirty(_mat_id);
        }
        else if (ReadFloat(paramValue, &emissionScale))
        {
          _scene.v_mat[_mat_id].emissionFactor = glm::vec3(emissionScale);
          _scene.v_mat[_mat_id].emission = glm::vec3(emissionScale);
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
      else if (paramName == "opacity")
      {
        if (ReadFloat(paramValue, &_scene.v_mat[_mat_id].opacityFactor))
        {
          _scene.v_mat[_mat_id].dissolve = _scene.v_mat[_mat_id].opacityFactor;
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
      else if (paramName == "transmission")
      {
        if (ReadFloat(paramValue, &_scene.v_mat[_mat_id].transmissionFactor))
        {
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
      else if (paramName == "transmission_color")
      {
        glm::vec3 transmissionColor;
        if (ReadVec3(paramValue, &transmissionColor))
        {
          _scene.v_mat[_mat_id].transmissionColorFactor = transmissionColor;
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
      else if (paramName == "subsurface")
      {
        if (ReadFloat(paramValue, &_scene.v_mat[_mat_id].subsurfaceFactor))
        {
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
      else if (paramName == "subsurface_color")
      {
        glm::vec3 subsurfaceColor;
        if (ReadVec3(paramValue, &subsurfaceColor))
        {
          _scene.v_mat[_mat_id].subsurfaceColorFactor = subsurfaceColor;
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
      else if (paramName == "subsurface_scale")
      {
        if (ReadFloat(paramValue, &_scene.v_mat[_mat_id].subsurfaceScale))
        {
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
    }
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
