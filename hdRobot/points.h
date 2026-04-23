#pragma once

#include "pxr/base/gf/matrix4d.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/vt/array.h"
#include "pxr/imaging/hd/points.h"
#include "renderParam.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotPoints final : public HdPoints
{
public:
  HdRobotPoints(const SdfPath& id, HdRobotRenderParam& scene);
  ~HdRobotPoints() override;

  void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits, TfToken const& reprToken) override;
  HdDirtyBits GetInitialDirtyBitsMask() const override;

protected:
  void _InitRepr(TfToken const& reprToken, HdDirtyBits* dirtyBits) override;
  HdDirtyBits _PropagateDirtyBits(HdDirtyBits bits) const override;

private:
  void _UpdateRenderScene() const;
  void _UpdateGeometry(HdSceneDelegate* sceneDelegate, HdDirtyBits* dirtyBits);
  void _UpdateTransform(HdSceneDelegate* sceneDelegate, HdDirtyBits* dirtyBits);
  void _UpdateVisibility(HdSceneDelegate* sceneDelegate, HdDirtyBits* dirtyBits);

  VtVec3fArray _points;
  VtFloatArray _widths;
  VtVec3fArray _colors;
  GfMatrix4d   _transform;
  bool         _visible;

  HdRobotRenderParam& _scene;
};

PXR_NAMESPACE_CLOSE_SCOPE
