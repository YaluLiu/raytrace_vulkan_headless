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

HdGatlingMaterial::HdGatlingMaterial(const SdfPath& id,
                             HdGatlingScene& scene)
  : HdMaterial(id)
  , _scene(scene)
{
}

void HdGatlingMaterial::Finalize(HdRenderParam* renderParam)
{
}

HdDirtyBits HdGatlingMaterial::GetInitialDirtyBitsMask() const
{
  return DirtyBits::DirtyParams;
}

void HdGatlingMaterial::Sync(HdSceneDelegate* sceneDelegate,
                             HdRenderParam* renderParam,
                             HdDirtyBits* dirtyBits)
{
    if (!TF_VERIFY(sceneDelegate)) return;

    HdDirtyBits bits = *dirtyBits;
    *dirtyBits = HdMaterial::Clean;

    if (!(bits & HdMaterial::DirtyResource)) {
        return;  // 材质不需要更新
    }

    // 获取 HdMaterialNetwork2 类型
    VtValue val = sceneDelegate->GetMaterialResource(GetId());
    if (!val.IsHolding<HdMaterialNetwork2>()) {
        TF_WARN("MaterialResource不是 HdMaterialNetwork2 类型: %s", GetId().GetText());
        return;
    }

    const HdMaterialNetwork2& net = val.UncheckedGet<HdMaterialNetwork2>();

    // net.nodes 是 map<SdfPath, HdMaterialNode2>
    for (const auto& [path, node] : net.nodes) {
        if (node.nodeTypeId == TfToken("UsdUVTexture")) {
            auto it = node.parameters.find(TfToken("file"));
            if (it != node.parameters.end()
                && it->second.IsHolding<SdfAssetPath>()) {
                
                const SdfAssetPath& assetPath = it->second.UncheckedGet<SdfAssetPath>();
                const std::string texturePath =
                    assetPath.GetResolvedPath().empty() ?
                    assetPath.GetAssetPath() :
                    assetPath.GetResolvedPath();

                std::cout << "Found texture at node " 
                          << path.GetName() 
                          << ": " << texturePath << std::endl;

                // TODO: 将 texturePath 缓存到类成员或进一步处理
            }
        }
    }
}


PXR_NAMESPACE_CLOSE_SCOPE
