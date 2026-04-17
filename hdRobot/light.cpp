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

#include "light.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/matrix3d.h>
#include <pxr/base/gf/matrix3f.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/quatf.h>
#include <pxr/imaging/hd/changeTracker.h>
#include <pxr/imaging/hd/sceneDelegate.h>
#include <pxr/imaging/glf/simpleLight.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usdLux/blackbody.h>

PXR_NAMESPACE_OPEN_SCOPE

namespace {
float _AreaEllipsoid(float radiusX, float radiusY, float radiusZ)
{
  float ab = powf(radiusX * radiusY, 1.6f);
  float ac = powf(radiusX * radiusZ, 1.6f);
  float bc = powf(radiusY * radiusZ, 1.6f);
  return powf((ab + ac + bc) / 3.0f, 1.0f / 1.6f) * 4.0f * M_PI;
}
}  // namespace

//
// Base Light
//

HdRobotLight::HdRobotLight(const SdfPath& id, HdRobotScene& scene)
    : HdLight(id)
    , _scene(scene)
{
  std::lock_guard guard(_scene.mutex);
  _light_id = _scene.v_light.size();
  _scene.v_light.emplace_back(HydraLight());
}

// We strive to conform to following UsdLux-enhancing specification:
// https://github.com/anderslanglands/light_comparison/blob/777ccc7afd1c174a5dcbbde964ced950eb3af11b/specification/specification.md
GfVec3f HdRobotLight::_CalcBaseEmission(HdSceneDelegate* sceneDelegate, float normalizeFactor = 1.0f)
{
  const SdfPath& id = GetId();

  VtValue boxedIntensity = sceneDelegate->GetLightParamValue(id, HdLightTokens->intensity);
  float   intensity      = boxedIntensity.GetWithDefault<float>(1.0f);

  VtValue boxedColor = sceneDelegate->GetLightParamValue(id, HdLightTokens->color);
  GfVec3f color      = boxedColor.GetWithDefault<GfVec3f>({1.0f, 1.0f, 1.0f});

  VtValue boxedEnableColorTemperature = sceneDelegate->GetLightParamValue(id, HdLightTokens->enableColorTemperature);
  bool    enableColorTemperature      = boxedEnableColorTemperature.GetWithDefault<bool>(false);

  VtValue boxedColorTemperature = sceneDelegate->GetLightParamValue(id, HdLightTokens->colorTemperature);
  float   colorTemperature      = boxedColorTemperature.GetWithDefault<float>(6500.0f);

  VtValue boxedExposureAttr = sceneDelegate->GetLightParamValue(id, HdLightTokens->exposure);
  float   exposure          = boxedExposureAttr.GetWithDefault<float>(0.0f);

  TF_AXIOM(normalizeFactor > 0.0f);

  float normalizedIntensity = intensity * powf(2.0f, exposure) / normalizeFactor;

  GfVec3f baseEmission = color * normalizedIntensity;

  if(enableColorTemperature)
  {
    baseEmission = GfCompMult(baseEmission, UsdLuxBlackbodyTemperatureAsRgb(colorTemperature));
  }

  return baseEmission;
}

HdDirtyBits HdRobotLight::GetInitialDirtyBitsMask() const
{
  return DirtyBits::DirtyParams | DirtyBits::DirtyTransform;
}

void HdRobotLight::Finalize([[maybe_unused]] HdRenderParam* renderParam)
{
  _scene.v_light[_light_id].valid = 0;
}

// --------Sphere Light-------------------------
HdRobotSphereLight::HdRobotSphereLight(const SdfPath& id, HdRobotScene& scene)
    : HdRobotLight(id, scene)
{
  _scene.v_light[_light_id].type = 0;
}

void HdRobotSphereLight::Sync(HdSceneDelegate* sceneDelegate, [[maybe_unused]] HdRenderParam* renderParam, HdDirtyBits* dirtyBits)
{
  const SdfPath&   id = GetId();
  const GfMatrix4f transform(sceneDelegate->GetTransform(id));
  _scene.v_light[_light_id].valid = 1;

  if(*dirtyBits & DirtyBits::DirtyTransform)
  {
    GfVec3f pos                        = transform.Transform(GfVec3f(0.0f, 0.0f, 0.0f));
    _scene.v_light[_light_id].position = glm::vec3(pos[0], pos[1], pos[2]);
  }

  if(*dirtyBits & DirtyBits::DirtyParams)
  {
    VtValue boxedRadius = sceneDelegate->GetLightParamValue(id, HdLightTokens->radius);
    float   radius      = boxedRadius.GetWithDefault<float>(0.5f);
    float   radiusX     = transform.TransformDir(GfVec3f{radius, 0.0f, 0.0f})[0];
    float   radiusY     = transform.TransformDir(GfVec3f{0.0f, radius, 0.0f})[1];
    float   radiusZ     = transform.TransformDir(GfVec3f{0.0f, 0.0f, radius})[2];

    VtValue boxedNormalize  = sceneDelegate->GetLightParamValue(id, HdLightTokens->normalize);
    bool    normalize       = boxedNormalize.GetWithDefault<bool>(false);
    float   area            = _AreaEllipsoid(radiusX, radiusY, radiusZ);
    float   normalizeFactor = (normalize && area > 0.0f) ? area : 1.0f;
    GfVec3f baseEmission    = _CalcBaseEmission(sceneDelegate, normalizeFactor);

    VtValue boxedDiffuse  = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse);
    float   diffuse       = boxedDiffuse.GetWithDefault<float>(1.0f);
    VtValue boxedSpecular = sceneDelegate->GetLightParamValue(id, HdLightTokens->specular);
    float   specular      = boxedSpecular.GetWithDefault<float>(1.0f);

    _scene.v_light[_light_id].baseEmission = glm::vec3(baseEmission[0], baseEmission[1], baseEmission[2]);
    _scene.v_light[_light_id].diffuse      = diffuse;
    _scene.v_light[_light_id].specular     = specular;
    _scene.v_light[_light_id].radius       = radius;
  }

  *dirtyBits = HdChangeTracker::Clean;
}

// --------Distant Light-------------------------
HdRobotDistantLight::HdRobotDistantLight(const SdfPath& id, HdRobotScene& scene)
    : HdRobotLight(id, scene)
{
  _scene.v_light[_light_id].type = 1;
}

void HdRobotDistantLight::Sync(HdSceneDelegate* sceneDelegate, [[maybe_unused]] HdRenderParam* renderParam, HdDirtyBits* dirtyBits)
{
  const SdfPath& id               = GetId();
  _scene.v_light[_light_id].valid = 1;
  if(*dirtyBits & DirtyBits::DirtyTransform)
  {
    const GfMatrix4f transform(sceneDelegate->GetTransform(id));
    GfMatrix4f       normalMatrix(transform.GetInverse().GetTranspose());

    GfVec3f dir = normalMatrix.TransformDir(GfVec3f(0.0f, 0.0f, -1.0f));
    // dir.Normalize();
    _scene.v_light[_light_id].direction = glm::vec3(dir[0], dir[1], dir[2]);
  }

  if(*dirtyBits & DirtyBits::DirtyParams)
  {
    VtValue boxedAngle = sceneDelegate->GetLightParamValue(id, HdLightTokens->angle);
    float   angle      = GfDegreesToRadians(boxedAngle.GetWithDefault<float>(0.53f));

    VtValue boxedNormalize  = sceneDelegate->GetLightParamValue(id, HdLightTokens->normalize);
    bool    normalize       = boxedNormalize.GetWithDefault<bool>(false);
    float   sinHalfAngle    = sinf(angle * 0.5f);
    float   normalizeFactor = (sinHalfAngle > 1.0e-6f && normalize) ? (GfSqr(sinHalfAngle) * M_PI) : 1.0f;
    GfVec3f baseEmission    = _CalcBaseEmission(sceneDelegate, normalizeFactor);

    VtValue boxedDiffuse  = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse);
    float   diffuse       = boxedDiffuse.GetWithDefault<float>(1.0f);
    VtValue boxedSpecular = sceneDelegate->GetLightParamValue(id, HdLightTokens->specular);
    float   specular      = boxedSpecular.GetWithDefault<float>(1.0f);

    _scene.v_light[_light_id].baseEmission = glm::vec3(baseEmission[0], baseEmission[1], baseEmission[2]);
    _scene.v_light[_light_id].diffuse      = diffuse;
    _scene.v_light[_light_id].specular     = specular;
    _scene.v_light[_light_id].angle        = angle;
  }

  *dirtyBits = HdChangeTracker::Clean;
}


// --------Dome Light-------------------------
HdRobotDomeLight::HdRobotDomeLight(const SdfPath& id, HdRobotScene& scene)
    : HdRobotLight(id, scene)
{
  _scene.v_light[_light_id].type = 2;
}

std::string HdRobotDomeLight::GetTexturePath(HdSceneDelegate* sceneDelegate)
{
  const SdfPath& id               = GetId();
  VtValue        boxedTextureFile = sceneDelegate->GetLightParamValue(id, HdLightTokens->textureFile);
  if(boxedTextureFile.IsEmpty())
  {
    // Hydra runtime warns of empty path; we don't need to repeat it.
    return "Error";
  }

  if(!boxedTextureFile.IsHolding<SdfAssetPath>())
  {
    TF_WARN("%s:%s does not hold SdfAssetPath - ignoring!", id.GetText(), HdLightTokens->textureFile.GetText());
    return "Error";
  }

  const SdfAssetPath& assetPath = boxedTextureFile.UncheckedGet<SdfAssetPath>();

  std::string path = assetPath.GetResolvedPath();
  if(path.empty())
  {
    // Texture file is missing
    TF_WARN("Unable to resolve asset path \"%s\"", assetPath.GetAssetPath().c_str());
    return "Error";
  }
  return path;
}

void HdRobotDomeLight::Sync(HdSceneDelegate* sceneDelegate, [[maybe_unused]] HdRenderParam* renderParam, HdDirtyBits* dirtyBits)
{
  const SdfPath& id               = GetId();
  _scene.v_light[_light_id].valid = 1;
  if(*dirtyBits & DirtyBits::DirtyTransform)
  {
    const GfMatrix4d& transform          = sceneDelegate->GetTransform(id);
    auto              rotateQuat         = GfMatrix4f(transform.GetOrthonormalized()).ExtractRotationQuat();
    _scene.v_light[_light_id].rotateQuat = glm::vec4(rotateQuat.GetImaginary()[0], rotateQuat.GetImaginary()[1],
                                                     rotateQuat.GetImaginary()[2], rotateQuat.GetImaginary()[3]);
  }

  if(*dirtyBits & DirtyBits::DirtyParams)
  {
    GfVec3f baseEmission  = _CalcBaseEmission(sceneDelegate);
    VtValue boxedDiffuse  = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse);
    float   diffuse       = boxedDiffuse.GetWithDefault<float>(1.0f);
    VtValue boxedSpecular = sceneDelegate->GetLightParamValue(id, HdLightTokens->specular);
    float   specular      = boxedSpecular.GetWithDefault<float>(1.0f);

    _scene.v_light[_light_id].baseEmission = glm::vec3(baseEmission[0], baseEmission[1], baseEmission[2]);
    _scene.v_light[_light_id].diffuse      = diffuse;
    _scene.v_light[_light_id].specular     = specular;
    std::string texturePath                = GetTexturePath(sceneDelegate);
    if(_scene.v_light[_light_id].texturePath != texturePath)
    {
      _scene.v_light[_light_id].texturePath = texturePath;
      _scene.v_light[_light_id].textureID   = _scene.RegisterTexturePath(texturePath);
    }
  }

  *dirtyBits = HdChangeTracker::Clean;
}

PXR_NAMESPACE_CLOSE_SCOPE
