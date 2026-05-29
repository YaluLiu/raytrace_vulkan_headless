#include "camera.h"

#include "renderParam.h"

#include <pxr/base/gf/camera.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/imaging/hd/sceneDelegate.h>

#include <algorithm>
#include <cmath>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
constexpr float kCameraEpsilon = 1.0e-6f;
} // namespace

HdRobotCameraData HdRobotComputeTransformCameraData(const SdfPath& id, const GfMatrix4d& transform)
{
  GfVec3d position = transform.Transform(GfVec3d(0.0, 0.0, 0.0));
  GfVec3d forward  = transform.TransformDir(GfVec3d(0.0, 0.0, -1.0));
  GfVec3d up       = transform.TransformDir(GfVec3d(0.0, 1.0, 0.0));

  forward.Normalize();
  up.Normalize();

  HdRobotCameraData data;
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

HdRobotCameraData HdRobotComputeCameraData(const HdCamera& camera)
{
  HdRobotCameraData data = HdRobotComputeTransformCameraData(camera.GetId(), camera.GetTransform());

  const float aperture    = camera.GetVerticalAperture() * GfCamera::APERTURE_UNIT;
  const float focalLength = camera.GetFocalLength() * GfCamera::FOCAL_LENGTH_UNIT;
  float       vfovDeg     = data.vfov_deg;
  if(camera.GetProjection() == HdCamera::Perspective && focalLength > kCameraEpsilon)
  {
    const float vfov = 2.0f * std::atan(aperture / (2.0f * focalLength));
    vfovDeg          = glm::degrees(vfov);
  }
  if(!std::isfinite(vfovDeg))
  {
    vfovDeg = 45.0f;
  }
  data.vfov_deg = std::clamp(vfovDeg, 1.0f, 179.0f);

  const GfRange1f clippingRange = camera.GetClippingRange();
  data.clipStart                = std::max(clippingRange.GetMin(), 0.001f);
  data.clipEnd                  = std::max(clippingRange.GetMax(), data.clipStart + 0.001f);

  return data;
}

HdRobotCamera::HdRobotCamera(const SdfPath& id, HdRobotRenderParam& scene, HdRobotCameraHandle handle)
    : HdCamera(id)
    , _scene(scene)
    , _handle(handle)
{
}

HdDirtyBits HdRobotCamera::GetInitialDirtyBitsMask() const
{
  return HdCamera::AllDirty;
}

const HdRobotCameraData& HdRobotCamera::GetCameraData() const
{
  return _cameraData;
}

void HdRobotCamera::Finalize(HdRenderParam*)
{
  _scene.GetBackendScene().DestroyCamera(_handle);
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

  HdRobotCameraData cameraData = HdRobotComputeCameraData(*this);
  _cameraData = cameraData;
  _scene.GetBackendScene().EnqueueCameraUpdate(HdRobotCameraUpdate{_handle, cameraData});
}

PXR_NAMESPACE_CLOSE_SCOPE
