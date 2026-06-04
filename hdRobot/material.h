#pragma once

#include <pxr/imaging/hd/material.h>
#include <pxr/usd/usdShade/material.h>
#include <pxr/usd/usdShade/shader.h>
#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/sceneDelegate.h>
#include "backendHandles.h"
#include "sceneData.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotSceneStore;

class HdRobotMaterial final : public HdMaterial
{
public:
  HdRobotMaterial(const SdfPath& id, HdRobotSceneStore& sceneStore, HdRobotMaterialHandle handle);

  void Finalize(HdRenderParam* renderParam) override;

public:
  HdDirtyBits GetInitialDirtyBitsMask() const override;

  void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;

  HdRobotMaterialHandle GetHandle() const;
  const HydraMaterial& GetMaterialData() const;
  const TfToken& GetBaseColorPrimvarName() const;

private:
  HdRobotSceneStore& _sceneStore;
  HdRobotMaterialHandle _handle;
  HydraMaterial        _materialData;
  TfToken              _baseColorPrimvarName;
};

PXR_NAMESPACE_CLOSE_SCOPE
