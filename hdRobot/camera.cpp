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

#include "camera.h"

#include <pxr/base/gf/camera.h>

#include <algorithm>
#include <cmath>

PXR_NAMESPACE_OPEN_SCOPE

namespace {
constexpr float kCameraEpsilon = 1.0e-6f;
}

HydraCamera HdGatlingComputeCameraData(const HdCamera& camera)
{
  const GfMatrix4d& transform = camera.GetTransform();

  GfVec3d position = transform.Transform(GfVec3d(0.0, 0.0, 0.0));
  GfVec3d forward  = transform.TransformDir(GfVec3d(0.0, 0.0, -1.0));
  GfVec3d up       = transform.TransformDir(GfVec3d(0.0, 1.0, 0.0));

  forward.Normalize();
  up.Normalize();

  HydraCamera data;
  data.position = glm::vec3(position[0], position[1], position[2]);
  data.forward  = glm::vec3(forward[0], forward[1], forward[2]);
  data.up       = glm::vec3(up[0], up[1], up[2]);

  if(glm::length(glm::cross(data.forward, data.up)) < kCameraEpsilon)
  {
    data.up = glm::vec3(0.0f, 1.0f, 0.0f);
  }

  const float aperture    = camera.GetVerticalAperture() * GfCamera::APERTURE_UNIT;
  const float focalLength = camera.GetFocalLength() * GfCamera::FOCAL_LENGTH_UNIT;
  float       vfov_deg    = data.vfov_deg;
  if(camera.GetProjection() == HdCamera::Perspective && focalLength > kCameraEpsilon)
  {
    float vfov = 2.0f * std::atan(aperture / (2.0f * focalLength));
    vfov_deg   = glm::degrees(vfov);
  }
  if(!std::isfinite(vfov_deg))
  {
    vfov_deg = 45.0f;
  }
  data.vfov_deg = std::clamp(vfov_deg, 1.0f, 179.0f);

  const GfRange1f clippingRange = camera.GetClippingRange();
  data.clipStart                = std::max(clippingRange.GetMin(), 0.001f);
  data.clipEnd                  = std::max(clippingRange.GetMax(), data.clipStart + 0.001f);

  data.valid = true;
  return data;
}

HdGatlingCamera::HdGatlingCamera(const SdfPath& id, HdGatlingScene& scene)
    : HdCamera(id)
    , _scene(scene)
{
}

HdDirtyBits HdGatlingCamera::GetInitialDirtyBitsMask() const
{
  return HdCamera::AllDirty;
}

void HdGatlingCamera::Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits)
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

  std::lock_guard guard(_scene.mutex);
  _scene.cameraCache[GetId().GetString()] = HdGatlingComputeCameraData(*this);
}

PXR_NAMESPACE_CLOSE_SCOPE
