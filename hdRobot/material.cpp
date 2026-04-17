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

HdRobotMaterial::HdRobotMaterial(const SdfPath& id, HdRobotScene& scene)
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

  // 遍历材质网络中的所有节点
  // network.nodes 是一个 map,存储了材质网络中所有的节点
  // key: SdfPath (节点路径), value: HdMaterialNode2 (节点数据)
  for(const auto& nodePair : network.nodes)
  {
    // 获取当前节点的路径(唯一标识符)
    const SdfPath& nodePath = nodePair.first;
    // 获取当前节点的数据(包含节点类型、参数、连接等信息)
    const HdMaterialNode2& node = nodePair.second;
    // 遍历当前节点的所有输入连接
    // inputConnections 描述了哪些输入是从其他节点连接过来的
    // 例如: diffuseColor 可能连接到一个纹理节点
    for(const auto& connPair : node.inputConnections)
    {
      // 输入端口的名称(如 "diffuseColor", "roughness" 等)
      const TfToken&                            inputName   = connPair.first;
      const std::vector<HdMaterialConnection2>& connections = connPair.second;

      if(inputName == "diffuseColor")
      {
        for(const auto& conn : connections)
        {
          // 获取上游节点的路径(提供数据的节点,如/_materials/default_002/preview/Image_Texture)
          const SdfPath& upstreamNodePath = conn.upstreamNode;
          // 上游节点的输出端口名称(如 "rgb", "result" 等)
          const TfToken& upstreamOutputName = conn.upstreamOutputName;

          // 在 network.nodes 中查找上游节点
          auto upstreamNodeIt = network.nodes.find(upstreamNodePath);
          if(upstreamNodeIt != network.nodes.end())
          {
            // 获取上游节点的数据
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
                    std::lock_guard guard(_scene.mutex);
                    _scene.v_mat[_mat_id].textureID = _scene.v_texturePath.size();
                    _scene.v_texturePath.emplace_back(texturePath);
                  }
                  // std::cout << "[material] _mat_id:" << _mat_id << "," << upstreamNodePath << ","
                  //           << _scene.v_mat[_mat_id].texturePath << std::endl;
                }
                else if(paramValue.IsHolding<std::string>())
                {
                  std::cout << "String = " << paramValue.Get<std::string>() << std::endl;
                }
              }
            }
          }
          else
          {
            std::cout << "  Warning: Upstream node not found!" << std::endl;
          }
        }
      }
    }

    // 遍历当前节点的所有参数parameters 存储了节点的属性值,比如颜色、粗糙度等
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
          _scene.v_mat[_mat_id].material_changed = true;
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
          _scene.v_mat[_mat_id].material_changed = true;
        }
      }
    }
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
