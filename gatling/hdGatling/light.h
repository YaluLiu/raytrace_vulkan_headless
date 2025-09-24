//
// Copyright (C) 2023 Pablo Delgado Krämer
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

#include <pxr/imaging/hd/light.h>
#include "renderScene.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdGatlingLight : public HdLight
{
public:
  HdGatlingLight(const SdfPath& id, HdGatlingScene& scene);

  HdDirtyBits GetInitialDirtyBitsMask() const override;

protected:
  GfVec3f _CalcBaseEmission(HdSceneDelegate* sceneDelegate, float normalizeFactor);

protected:
  HdGatlingScene& _scene;
  int             _light_id;
};

class HdGatlingDistantLight final : public HdGatlingLight
{
public:
  HdGatlingDistantLight(const SdfPath& id, HdGatlingScene& scene);

public:
  void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;

  void Finalize(HdRenderParam* renderParam) override;
};

class HdGatlingSphereLight final : public HdGatlingLight
{
public:
  HdGatlingSphereLight(const SdfPath& id, HdGatlingScene& scene);

public:
  void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;

  void Finalize(HdRenderParam* renderParam) override;
};

class HdGatlingDomeLight final : public HdGatlingLight
{
public:
  HdGatlingDomeLight(const SdfPath& id, HdGatlingScene& scene);

public:
  void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;

  void Finalize(HdRenderParam* renderParam) override;
};

PXR_NAMESPACE_CLOSE_SCOPE
