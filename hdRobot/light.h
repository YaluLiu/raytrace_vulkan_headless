#pragma once

#include <pxr/imaging/hd/light.h>
#include "backendHandles.h"
#include "sceneData.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotBackendScene;

class HdRobotLight : public HdLight
{
public:
  HdRobotLight(const SdfPath& id, HdRobotBackendScene& backendScene, HdRobotLightHandle handle);

  HdDirtyBits GetInitialDirtyBitsMask() const override;

protected:
  GfVec3f _CalcBaseEmission(HdSceneDelegate* sceneDelegate, float normalizeFactor);
  bool    _CanSyncLight() const;
  void    _EnqueueLightUpdate(bool active = true);
  void    Finalize(HdRenderParam* renderParam) override;

protected:
  HdRobotBackendScene& _backendScene;
  HdRobotLightHandle  _handle;
  HydraLight          _lightData;
};

class HdRobotDistantLight final : public HdRobotLight
{
public:
  HdRobotDistantLight(const SdfPath& id, HdRobotBackendScene& backendScene, HdRobotLightHandle handle);

public:
  void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;
};

class HdRobotSphereLight final : public HdRobotLight
{
public:
  HdRobotSphereLight(const SdfPath& id, HdRobotBackendScene& backendScene, HdRobotLightHandle handle);

public:
  void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;
};

class HdRobotSimpleLight final : public HdRobotLight
{
public:
  HdRobotSimpleLight(const SdfPath& id, HdRobotBackendScene& backendScene, HdRobotLightHandle handle);

public:
  void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;
};

class HdRobotDomeLight final : public HdRobotLight
{
public:
  HdRobotDomeLight(const SdfPath& id, HdRobotBackendScene& backendScene, HdRobotLightHandle handle);

public:
  std::string GetTexturePath(HdSceneDelegate* sceneDelegate);
  void        Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;
};

PXR_NAMESPACE_CLOSE_SCOPE
