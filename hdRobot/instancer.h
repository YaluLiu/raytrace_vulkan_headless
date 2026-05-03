#pragma once

#include <pxr/imaging/hd/instancer.h>
#include "utils.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotInstancer final : public HdInstancer
{
public:
  HdRobotInstancer(HdSceneDelegate* delegate, const SdfPath& id);

  ~HdRobotInstancer() override;

public:
  VtMatrix4fArray ComputeFlattenedTransforms(const SdfPath& prototypeId);

  std::vector<GiPrimvarData> ComputeFlattenedPrimvars(const SdfPath& prototypeId);

  void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits) override;

private:
  std::vector<GiPrimvarData> MakeGiPrimvars(const SdfPath& prototypeId);

  TfHashMap<TfToken, VtValue, TfToken::HashFunctor> _primvarMap;
};

PXR_NAMESPACE_CLOSE_SCOPE
