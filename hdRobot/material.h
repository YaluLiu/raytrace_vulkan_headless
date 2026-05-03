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

