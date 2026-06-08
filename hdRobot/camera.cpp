#include "camera.h"

#include "sceneStore.h"

#include <pxr/base/gf/camera.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include <algorithm>
#include <cmath>

PXR_NAMESPACE_OPEN_SCOPE

using hdrobot::CameraData;
using hdrobot::CameraHandle;
using hdrobot::CameraUpdate;
using hdrobot::SceneStore;

namespace
{
constexpr float kCameraEpsilon = 1.0e-6f;

CameraConformPolicy ToCameraConformPolicy(CameraUtilConformWindowPolicy policy)
{
  switch(policy)
  {
    case CameraUtilMatchVertically:
      return CameraConformPolicy::MatchVertically;
    case CameraUtilMatchHorizontally:
      return CameraConformPolicy::MatchHorizontally;
    case CameraUtilFit:
      return CameraConformPolicy::Fit;
    case CameraUtilCrop:
      return CameraConformPolicy::Crop;
    case CameraUtilDontConform:
      return CameraConformPolicy::DontConform;
  }
  return CameraConformPolicy::Fit;
}
} // namespace

CameraData HdRobotComputeTransformCameraData(const SdfPath& id, const GfMatrix4d& transform)
{
  GfVec3d position = transform.Transform(GfVec3d(0.0, 0.0, 0.0));
  GfVec3d forward  = transform.TransformDir(GfVec3d(0.0, 0.0, -1.0));
  GfVec3d up       = transform.TransformDir(GfVec3d(0.0, 1.0, 0.0));

  forward.Normalize();
  up.Normalize();

  CameraData data;
  data.name     = id.GetString();
  data.position = glm::vec3(position[0], position[1], position[2]);
  data.forward  = glm::vec3(forward[0], forward[1], forward[2]);
  data.up       = glm::vec3(up[0], up[1], up[2]);

  if(glm::length(glm::cross(data.forward, data.up)) < kCameraEpsilon)
  {
    data.up = glm::vec3(0.0f, 1.0f, 0.0f);
  }

  return data;
}

CameraData HdRobotComputeCameraData(const HdCamera& camera)
{
  CameraData data = HdRobotComputeTransformCameraData(camera.GetId(), camera.GetTransform());

  const float verticalAperture = camera.GetVerticalAperture() * GfCamera::APERTURE_UNIT;
  const float horizontalAperture = camera.GetHorizontalAperture() * GfCamera::APERTURE_UNIT;
  const float focalLength = camera.GetFocalLength() * GfCamera::FOCAL_LENGTH_UNIT;
  float       verticalFovDegrees   = data.verticalFovDegrees;
  float       horizontalFovDegrees = data.horizontalFovDegrees;
  if(camera.GetProjection() == HdCamera::Perspective && focalLength > kCameraEpsilon)
  {
    const float verticalFov = 2.0f * std::atan(verticalAperture / (2.0f * focalLength));
    verticalFovDegrees = glm::degrees(verticalFov);
    const float horizontalFov = 2.0f * std::atan(horizontalAperture / (2.0f * focalLength));
    horizontalFovDegrees = glm::degrees(horizontalFov);
  }
  if(!std::isfinite(verticalFovDegrees))
  {
    verticalFovDegrees = 45.0f;
  }
  if(!std::isfinite(horizontalFovDegrees))
  {
    horizontalFovDegrees = 0.0f;
  }
  data.verticalFovDegrees = std::clamp(verticalFovDegrees, 1.0f, 179.0f);
  data.horizontalFovDegrees =
      horizontalFovDegrees > 0.0f ? std::clamp(horizontalFovDegrees, 1.0f, 179.0f) : 0.0f;
  data.conformPolicy = ToCameraConformPolicy(camera.GetWindowPolicy());

  const GfRange1f clippingRange = camera.GetClippingRange();
  data.clipStart                = std::max(clippingRange.GetMin(), 0.001f);
  data.clipEnd                  = std::max(clippingRange.GetMax(), data.clipStart + 0.001f);

  return data;
}

HdRobotCamera::HdRobotCamera(const SdfPath& id, SceneStore& sceneStore, CameraHandle handle)
    : HdCamera(id)
    , _sceneStore(sceneStore)
    , _handle(handle)
{
}

HdDirtyBits HdRobotCamera::GetInitialDirtyBitsMask() const
{
  return HdCamera::AllDirty;
}

const CameraData& HdRobotCamera::GetCameraData() const
{
  return _cameraData;
}

void HdRobotCamera::Finalize(HdRenderParam*)
{
  _sceneStore.DestroyCamera(_handle);
}

void HdRobotCamera::Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits)
{
  if(!dirtyBits)
  {
    return;
  }

  const HdDirtyBits bits = *dirtyBits;
  HdCamera::Sync(sceneDelegate, renderParam, dirtyBits);

  if((bits & (DirtyBits::DirtyTransform | DirtyBits::DirtyParams)) == 0)
  {
    return;
  }

  CameraData cameraData = HdRobotComputeCameraData(*this);
  _cameraData = cameraData;
  _sceneStore.EnqueueCameraUpdate(CameraUpdate{_handle, cameraData});
}

PXR_NAMESPACE_CLOSE_SCOPE
