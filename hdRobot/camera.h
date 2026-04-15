//
// Copyright (C) 2026 Pablo Delgado Krämer
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

#include <pxr/imaging/hd/camera.h>
#include "renderScene.h"

PXR_NAMESPACE_OPEN_SCOPE

HydraCamera HdGatlingComputeCameraData(const HdCamera& camera);

class HdGatlingCamera final : public HdCamera
{
public:
  HdGatlingCamera(const SdfPath& id, HdGatlingScene& scene);

public:
  HdDirtyBits GetInitialDirtyBitsMask() const override;
  void        Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;

private:
  HdGatlingScene& _scene;
};

PXR_NAMESPACE_CLOSE_SCOPE
