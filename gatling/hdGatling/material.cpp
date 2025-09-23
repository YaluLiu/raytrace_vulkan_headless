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

void HdGatlingMaterial::Finalize(HdRenderParam* renderParam) {}

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
    return;  // 材质不需要更新
  }

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

  // 修正遍历方式
  for(const auto& nodePair : network.nodes)
  {
    const SdfPath&         nodePath = nodePair.first;
    const HdMaterialNode2& node     = nodePair.second;
    // std::cout << "[material]Node: " << nodePath.GetString() << ", NodeType: " << node.nodeTypeId << std::endl;

    for(const auto& paramPair : node.parameters)
    {
      const TfToken& paramName  = paramPair.first;
      const VtValue& paramValue = paramPair.second;

      // 专门处理 diffuseColor 参数
      if(paramName == "diffuseColor")
      {
        // 检查是否为 GfVec3f 类型
        if(paramValue.IsHolding<GfVec3f>())
        {
          auto        diffuse_color = paramValue.Get<GfVec3f>();
          MaterialObj mat;
          mat.diffuse[0]        = diffuse_color[0];
          mat.diffuse[1]        = diffuse_color[1];
          mat.diffuse[2]        = diffuse_color[2];
          mat.material_changed  = true;
          _scene.v_mat[_mat_id] = mat;
          // std::cout << "[mat]" << _mat_id << "changed!" << std::endl;
        }
        // 检查是否为 GfVec4f 类型 (带alpha通道)
        else if(paramValue.IsHolding<GfVec4f>())
        {
          GfVec4f color = paramValue.Get<GfVec4f>();
          std::cerr << "[material] diffuseColor (GfVec4f): R=" << color[0] << ", G=" << color[1] << ", B=" << color[2]
                    << ", A=" << color[3] << std::endl;
        }
        // 检查其他可能的颜色类型
        else
        {
          std::cerr << "diffuseColor has unexpected type: " << paramValue.GetTypeName() << std::endl;
          std::cerr << "Raw value: " << paramValue << std::endl;
        }
      }

      // 打印 inputConnections（参数与贴图的连接）
      for(const auto& connPair : node.inputConnections)
      {
        const TfToken&                            paramName   = connPair.first;
        const std::vector<HdMaterialConnection2>& connections = connPair.second;
        for(const auto& conn : connections)
        {
          // HdMaterialConnection2 通常包含 .upstreamNode（SdfPath）和 .upstreamOutputName（TfToken）
          std::cout << "[material]" << paramName.GetString() << " <- " << conn.upstreamNode.GetString() << "."
                    << conn.upstreamOutputName.GetString() << std::endl;
        }
      }
    }
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
