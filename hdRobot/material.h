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

#pragma once

#include <pxr/imaging/hd/material.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/sceneDelegate.h>
#include "renderParam.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotMaterial final : public HdMaterial
{
public:
  HdRobotMaterial(const SdfPath& id, HdRobotRenderParam& _scene);

  void Finalize(HdRenderParam* renderParam) override;

public:
  HdDirtyBits GetInitialDirtyBitsMask() const override;

  void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;

public:
  HdRobotRenderParam& _scene;
  int             _mat_id;
};

PXR_NAMESPACE_CLOSE_SCOPE

