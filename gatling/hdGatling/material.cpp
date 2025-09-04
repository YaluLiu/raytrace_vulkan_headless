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
    std::cout << "---------------------------------------------------" << std::endl;
    std::cout << "Node: " << nodePath.GetString() << ", NodeType: " << node.nodeTypeId << std::endl;

    // 打印参数
    for(const auto& paramPair : node.parameters)
    {
      const TfToken& paramName  = paramPair.first;
      const VtValue& paramValue = paramPair.second;
      std::cout << paramName.GetString() << "(" << paramValue.GetTypeName().c_str() << ") = " << paramValue << std::endl;
    }

    // 打印 inputConnections（参数与贴图的连接）
    for(const auto& connPair : node.inputConnections)
    {
      const TfToken&                            paramName   = connPair.first;
      const std::vector<HdMaterialConnection2>& connections = connPair.second;
      for(const auto& conn : connections)
      {
        // HdMaterialConnection2 通常包含 .upstreamNode（SdfPath）和 .upstreamOutputName（TfToken）
        std::cout << paramName.GetString() << " <- " << conn.upstreamNode.GetString() << "."
                  << conn.upstreamOutputName.GetString() << std::endl;
      }
    }
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
