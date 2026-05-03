//
// Copyright (C) 2019-2022 Pablo Delgado Krämer
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "material.h"
#include "pxr/imaging/hd/material.h"
#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/usd/sdf/assetPath.h"

PXR_NAMESPACE_OPEN_SCOPE

HdRobotMaterial::HdRobotMaterial(const SdfPath& id, HdRobotRenderParam& scene)
    : HdMaterial(id)
    , _scene(scene)
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
  if(!TF_VERIFY(sceneDelegate))
    return;
  bool pullMaterial = (*dirtyBits & DirtyBits::DirtyParams);

  *dirtyBits = DirtyBits::Clean;

  if(!pullMaterial)
  {
    return;
  }

  _scene.v_mat[_mat_id].set_default();
  const SdfPath& id       = GetId();
  const VtValue& resource = sceneDelegate->GetMaterialResource(id);

  if(!resource.IsHolding<HdMaterialNetworkMap>())
  {
    return;
  }

  const HdMaterialNetworkMap& networkMap = resource.UncheckedGet<HdMaterialNetworkMap>();
  bool                        isVolume   = false;

  HdMaterialNetwork2 network = HdConvertToHdMaterialNetwork2(networkMap, &isVolume);
  if(isVolume)
  {
    TF_WARN("Volume %s unsupported", id.GetText());
    return;
  }

  for(const auto& nodePair : network.nodes)
  {
    const SdfPath& nodePath = nodePair.first;
    const HdMaterialNode2& node = nodePair.second;
    for(const auto& connPair : node.inputConnections)
    {
      const TfToken&                            inputName   = connPair.first;
      const std::vector<HdMaterialConnection2>& connections = connPair.second;

      if(inputName == "diffuseColor")
      {
        for(const auto& conn : connections)
        {
          const SdfPath& upstreamNodePath = conn.upstreamNode;
          const TfToken& upstreamOutputName = conn.upstreamOutputName;

          auto upstreamNodeIt = network.nodes.find(upstreamNodePath);
          if(upstreamNodeIt != network.nodes.end())
          {
            const HdMaterialNode2& upstreamNode = upstreamNodeIt->second;
            for(const auto& paramPair : upstreamNode.parameters)
            {
              // sourceColorSpace:sRGB
              // file:./textures/texture_pbr_v128.png
              const TfToken& paramName  = paramPair.first;
              const VtValue& paramValue = paramPair.second;

              if(paramName == "file" || paramName == "filename")
              {
                if(paramValue.IsHolding<SdfAssetPath>())
                {
                  SdfAssetPath assetPath   = paramValue.Get<SdfAssetPath>();
                  auto         texturePath = assetPath.GetResolvedPath();
                  if(_scene.v_mat[_mat_id].texturePath != texturePath)
                  {
                    _scene.v_mat[_mat_id].texturePath = texturePath;  //GetResolvedPath
                    _scene.v_mat[_mat_id].textureID   = _scene.RegisterTexturePath(texturePath);
                  }
                }
              }
            }
          }
        }
      }
    }

    for(const auto& paramPair : node.parameters)
    {
      const TfToken& paramName  = paramPair.first;
      const VtValue& paramValue = paramPair.second;
      if(paramName == "diffuseColor")
      {
        if(paramValue.IsHolding<GfVec3f>())
        {
          auto diffuse_color                     = paramValue.Get<GfVec3f>();
          _scene.v_mat[_mat_id].diffuse[0]       = diffuse_color[0];
          _scene.v_mat[_mat_id].diffuse[1]       = diffuse_color[1];
          _scene.v_mat[_mat_id].diffuse[2]       = diffuse_color[2];
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
      else if(paramName == "emissiveColor")
      {
        if(paramValue.IsHolding<GfVec3f>())
        {
          auto emission                          = paramValue.Get<GfVec3f>();
          _scene.v_mat[_mat_id].emission[0]      = emission[0];
          _scene.v_mat[_mat_id].emission[1]      = emission[1];
          _scene.v_mat[_mat_id].emission[2]      = emission[2];
          _scene.MarkMaterialDirty(_mat_id);
        }
      }
    }
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
