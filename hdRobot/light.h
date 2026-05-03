#pragma once

#include <pxr/imaging/hd/light.h>
#include "renderParam.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotLight : public HdLight
{
public:
  HdRobotLight(const SdfPath& id, HdRobotRenderParam& scene);

  HdDirtyBits GetInitialDirtyBitsMask() const override;

protected:
  GfVec3f _CalcBaseEmission(HdSceneDelegate* sceneDelegate, float normalizeFactor);
  void    Finalize(HdRenderParam* renderParam) override;

protected:
  HdRobotRenderParam& _scene;
  int             _light_id;
};

class HdRobotDistantLight final : public HdRobotLight
{
public:
  HdRobotDistantLight(const SdfPath& id, HdRobotRenderParam& scene);

public:
  void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;
};

class HdRobotSphereLight final : public HdRobotLight
{
public:
  HdRobotSphereLight(const SdfPath& id, HdRobotRenderParam& scene);

public:
  void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;
};

class HdRobotDomeLight final : public HdRobotLight
{
public:
  HdRobotDomeLight(const SdfPath& id, HdRobotRenderParam& scene);

public:
  std::string GetTexturePath(HdSceneDelegate* sceneDelegate);
  void        Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;
};

PXR_NAMESPACE_CLOSE_SCOPE

