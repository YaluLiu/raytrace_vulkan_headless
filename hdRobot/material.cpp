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

HdGatlingMaterial::HdGatlingMaterial(const SdfPath& id, HdGatlingScene& scene)
    : HdMaterial(id)
    , _scene(scene)
{
  std::lock_guard guard(_scene.mutex);
  _mat_id = _scene.v_mat.size();
  _scene.v_mat.emplace_back(MaterialObj());
}

void HdGatlingMaterial::Finalize(HdRenderParam* renderParam)
{
  _scene.v_mat[_mat_id].set_default();
}

HdDirtyBits HdGatlingMaterial::GetInitialDirtyBitsMask() const
{
  return DirtyBits::DirtyParams;
}

void HdGatlingMaterial::Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits)
{
  if(!TF_VERIFY(sceneDelegate))
    return;

  HdDirtyBits bits = *dirtyBits;
  *dirtyBits       = HdMaterial::Clean;

  if(!(bits & HdMaterial::DirtyResource))
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
    const SdfPath&         nodePath = nodePair.first;
    const HdMaterialNode2& node     = nodePair.second;
    std::cout << "-----Loop:" << nodePath << "---------" << std::endl;
    for(const auto& connPair : node.inputConnections)
    {
      const TfToken&                            inputName   = connPair.first;
      const std::vector<HdMaterialConnection2>& connections = connPair.second;

      if(inputName == "diffuseColor")
      {
        std::cout << "Found diffuseColor connection!" << std::endl;

        for(const auto& conn : connections)
        {
          // 获取上游节点的路径
          const SdfPath& upstreamNodePath   = conn.upstreamNode;
          const TfToken& upstreamOutputName = conn.upstreamOutputName;

          std::cout << "  Connected to node: " << upstreamNodePath << std::endl;
          std::cout << "  Output name: " << upstreamOutputName << std::endl;

          // 查找上游节点
          auto upstreamNodeIt = network.nodes.find(upstreamNodePath);
          if(upstreamNodeIt != network.nodes.end())
          {
            const HdMaterialNode2& upstreamNode = upstreamNodeIt->second;

            std::cout << "  Upstream node type: " << upstreamNode.nodeTypeId << std::endl;

            // 打印上游节点的所有参数
            std::cout << "  Upstream node parameters:" << std::endl;
            for(const auto& paramPair : upstreamNode.parameters)
            {
              const TfToken& paramName  = paramPair.first;
              const VtValue& paramValue = paramPair.second;

              std::cout << "    " << paramName << ": ";

              // 检查是否是文件路径
              if(paramName == "file" || paramName == "filename")
              {
                if(paramValue.IsHolding<SdfAssetPath>())
                {
                  SdfAssetPath assetPath = paramValue.Get<SdfAssetPath>();
                  std::cout << "AssetPath = " << assetPath.GetAssetPath() << std::endl;
                }
                else if(paramValue.IsHolding<std::string>())
                {
                  std::cout << "String = " << paramValue.Get<std::string>() << std::endl;
                }
                else
                {
                  std::cout << paramValue.GetTypeName() << std::endl;
                }
              }
              else
              {
                std::cout << paramValue.GetTypeName() << std::endl;
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

    for(const auto& paramPair : node.parameters)
    {
      const TfToken& paramName  = paramPair.first;
      const VtValue& paramValue = paramPair.second;
      // 检查输入连接
      if(paramName == "diffuseColor")
      {
        if(paramValue.IsHolding<GfVec3f>())
        {
          auto        diffuse_color = paramValue.Get<GfVec3f>();
          MaterialObj mat;
          mat.diffuse[0]        = diffuse_color[0];
          mat.diffuse[1]        = diffuse_color[1];
          mat.diffuse[2]        = diffuse_color[2];
          mat.material_changed  = true;
          _scene.v_mat[_mat_id] = mat;
        }
      }
    }
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
