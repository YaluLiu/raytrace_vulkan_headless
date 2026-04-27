//
// Copyright (C) 2019-2022 Pablo Delgado Kr盲mer
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

  // 閬嶅巻鏉愯川缃戠粶涓殑鎵€鏈夎妭鐐?
  // network.nodes 鏄竴涓?map,瀛樺偍浜嗘潗璐ㄧ綉缁滀腑鎵€鏈夌殑鑺傜偣
  // key: SdfPath (鑺傜偣璺緞), value: HdMaterialNode2 (鑺傜偣鏁版嵁)
  for(const auto& nodePair : network.nodes)
  {
    // 鑾峰彇褰撳墠鑺傜偣鐨勮矾寰?鍞竴鏍囪瘑绗?
    const SdfPath& nodePath = nodePair.first;
    // 鑾峰彇褰撳墠鑺傜偣鐨勬暟鎹?鍖呭惈鑺傜偣绫诲瀷銆佸弬鏁般€佽繛鎺ョ瓑淇℃伅)
    const HdMaterialNode2& node = nodePair.second;
    // 閬嶅巻褰撳墠鑺傜偣鐨勬墍鏈夎緭鍏ヨ繛鎺?
    // inputConnections 鎻忚堪浜嗗摢浜涜緭鍏ユ槸浠庡叾浠栬妭鐐硅繛鎺ヨ繃鏉ョ殑
    // 渚嬪: diffuseColor 鍙兘杩炴帴鍒颁竴涓汗鐞嗚妭鐐?
    for(const auto& connPair : node.inputConnections)
    {
      // 杈撳叆绔彛鐨勫悕绉?濡?"diffuseColor", "roughness" 绛?
      const TfToken&                            inputName   = connPair.first;
      const std::vector<HdMaterialConnection2>& connections = connPair.second;

      if(inputName == "diffuseColor")
      {
        for(const auto& conn : connections)
        {
          // 鑾峰彇涓婃父鑺傜偣鐨勮矾寰?鎻愪緵鏁版嵁鐨勮妭鐐?濡?_materials/default_002/preview/Image_Texture)
          const SdfPath& upstreamNodePath = conn.upstreamNode;
          // 涓婃父鑺傜偣鐨勮緭鍑虹鍙ｅ悕绉?濡?"rgb", "result" 绛?
          const TfToken& upstreamOutputName = conn.upstreamOutputName;

          // 鍦?network.nodes 涓煡鎵句笂娓歌妭鐐?
          auto upstreamNodeIt = network.nodes.find(upstreamNodePath);
          if(upstreamNodeIt != network.nodes.end())
          {
            // 鑾峰彇涓婃父鑺傜偣鐨勬暟鎹?
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
                  // std::cout << "[material] _mat_id:" << _mat_id << "," << upstreamNodePath << ","
                  //           << _scene.v_mat[_mat_id].texturePath << std::endl;
                }
              }
            }
          }
        }
      }
    }

    // 閬嶅巻褰撳墠鑺傜偣鐨勬墍鏈夊弬鏁皃arameters 瀛樺偍浜嗚妭鐐圭殑灞炴€у€?姣斿棰滆壊銆佺矖绯欏害绛?
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

