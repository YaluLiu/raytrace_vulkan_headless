#include "light.h"

#include "sceneStore.h"

#include <pxr/base/gf/matrix3d.h>
#include <pxr/base/gf/matrix3f.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/matrix4f.h>
#include <pxr/base/gf/quatf.h>
#include <pxr/imaging/glf/simpleLight.h>
#include <pxr/imaging/hd/changeTracker.h>
#include <pxr/imaging/hd/sceneDelegate.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usdLux/blackbody.h>

#include <algorithm>

PXR_NAMESPACE_OPEN_SCOPE

using hdrobot::IsValid;
using hdrobot::LightHandle;
using hdrobot::LightUpdate;
using hdrobot::SceneStore;

namespace
{
float _AreaEllipsoid(float radiusX, float radiusY, float radiusZ)
{
  float ab = powf(radiusX * radiusY, 1.6f);
  float ac = powf(radiusX * radiusZ, 1.6f);
  float bc = powf(radiusY * radiusZ, 1.6f);
  return powf((ab + ac + bc) / 3.0f, 1.0f / 1.6f) * 4.0f * M_PI;
}

float _MaxRgb(const GfVec4f& color)
{
  return std::max(color[0], std::max(color[1], color[2]));
}
}  // namespace

//
// Base Light
//

HdRobotLight::HdRobotLight(const SdfPath& id, SceneStore& sceneStore, LightHandle handle)
    : HdLight(id)
    , _sceneStore(sceneStore)
    , _handle(handle)
{
}

// We strive to conform to following UsdLux-enhancing specification:
// https://github.com/anderslanglands/light_comparison/blob/777ccc7afd1c174a5dcbbde964ced950eb3af11b/specification/specification.md
GfVec3f HdRobotLight::_CalcBaseEmission(HdSceneDelegate* sceneDelegate, float normalizeFactor = 1.0f)
{
  const SdfPath& id = GetId();

  VtValue boxedIntensity = sceneDelegate->GetLightParamValue(id, HdLightTokens->intensity);
  float intensity = boxedIntensity.GetWithDefault<float>(1.0f);

  VtValue boxedColor = sceneDelegate->GetLightParamValue(id, HdLightTokens->color);
  GfVec3f color = boxedColor.GetWithDefault<GfVec3f>({1.0f, 1.0f, 1.0f});

  VtValue boxedEnableColorTemperature = sceneDelegate->GetLightParamValue(id, HdLightTokens->enableColorTemperature);
  bool enableColorTemperature = boxedEnableColorTemperature.GetWithDefault<bool>(false);

  VtValue boxedColorTemperature = sceneDelegate->GetLightParamValue(id, HdLightTokens->colorTemperature);
  float colorTemperature = boxedColorTemperature.GetWithDefault<float>(6500.0f);

  VtValue boxedExposureAttr = sceneDelegate->GetLightParamValue(id, HdLightTokens->exposure);
  float exposure = boxedExposureAttr.GetWithDefault<float>(0.0f);

  TF_AXIOM(normalizeFactor > 0.0f);

  float normalizedIntensity = intensity * powf(2.0f, exposure) / normalizeFactor;

  GfVec3f baseEmission = color * normalizedIntensity;

  if (enableColorTemperature)
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
  _sceneStore.DestroyLight(_handle);
}

bool HdRobotLight::_CanSyncLight() const
{
  return IsValid(_handle);
}

void HdRobotLight::_EnqueueLightUpdate(bool active)
{
  _sceneStore.EnqueueLightUpdate(LightUpdate{_handle, _lightData, active});
}

// --------Sphere Light-------------------------
HdRobotSphereLight::HdRobotSphereLight(const SdfPath& id, SceneStore& sceneStore, LightHandle handle)
    : HdRobotLight(id, sceneStore, handle)
{
}

void HdRobotSphereLight::Sync(HdSceneDelegate* sceneDelegate, [[maybe_unused]] HdRenderParam* renderParam,
                              HdDirtyBits* dirtyBits)
{
  if(!_CanSyncLight())
  {
    *dirtyBits = HdChangeTracker::Clean;
    return;
  }

  const SdfPath& id = GetId();
  const GfMatrix4f transform(sceneDelegate->GetTransform(id));
  HydraLight light = _lightData;
  light.type = 0;
  light.valid = 1;

  if (*dirtyBits & DirtyBits::DirtyTransform)
  {
    GfVec3f pos = transform.Transform(GfVec3f(0.0f, 0.0f, 0.0f));
    light.position = glm::vec3(pos[0], pos[1], pos[2]);
  }

  if (*dirtyBits & DirtyBits::DirtyParams)
  {
    VtValue boxedRadius = sceneDelegate->GetLightParamValue(id, HdLightTokens->radius);
    float radius = boxedRadius.GetWithDefault<float>(0.5f);
    float radiusX = transform.TransformDir(GfVec3f{radius, 0.0f, 0.0f})[0];
    float radiusY = transform.TransformDir(GfVec3f{0.0f, radius, 0.0f})[1];
    float radiusZ = transform.TransformDir(GfVec3f{0.0f, 0.0f, radius})[2];

    VtValue boxedNormalize = sceneDelegate->GetLightParamValue(id, HdLightTokens->normalize);
    bool normalize = boxedNormalize.GetWithDefault<bool>(false);
    float area = _AreaEllipsoid(radiusX, radiusY, radiusZ);
    float normalizeFactor = (normalize && area > 0.0f) ? area : 1.0f;
    GfVec3f baseEmission = _CalcBaseEmission(sceneDelegate, normalizeFactor);

    VtValue boxedDiffuse = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse);
    float diffuse = boxedDiffuse.GetWithDefault<float>(1.0f);
    VtValue boxedSpecular = sceneDelegate->GetLightParamValue(id, HdLightTokens->specular);
    float specular = boxedSpecular.GetWithDefault<float>(1.0f);

    light.baseEmission = glm::vec3(baseEmission[0], baseEmission[1], baseEmission[2]);
    light.diffuse = diffuse;
    light.specular = specular;
    light.radius = radius;
  }

  _lightData = light;
  _EnqueueLightUpdate();
  *dirtyBits = HdChangeTracker::Clean;
}

// --------Simple Light-------------------------
HdRobotSimpleLight::HdRobotSimpleLight(const SdfPath& id, SceneStore& sceneStore, LightHandle handle)
    : HdRobotLight(id, sceneStore, handle)
{
}

void HdRobotSimpleLight::Sync(HdSceneDelegate* sceneDelegate, [[maybe_unused]] HdRenderParam* renderParam,
                              HdDirtyBits* dirtyBits)
{
  if(!_CanSyncLight())
  {
    *dirtyBits = HdChangeTracker::Clean;
    return;
  }

  const SdfPath& id = GetId();
  VtValue boxedSimpleLight = sceneDelegate->Get(id, HdLightTokens->params);
  if(!boxedSimpleLight.IsHolding<GlfSimpleLight>())
  {
    TF_WARN("%s:%s does not hold GlfSimpleLight - ignoring simpleLight", id.GetText(),
            HdLightTokens->params.GetText());
    _lightData.valid = 0;
    _EnqueueLightUpdate();
    *dirtyBits = HdChangeTracker::Clean;
    return;
  }

  const GlfSimpleLight simpleLight = boxedSimpleLight.Get<GlfSimpleLight>();
  HydraLight           light       = _lightData;
  light.valid                      = simpleLight.HasIntensity() ? 1 : 0;
  light.type                       = 0;

  const GfVec4f position = simpleLight.GetPosition();
  const GfVec4f diffuse  = simpleLight.GetDiffuse();
  const GfVec4f specular = simpleLight.GetSpecular();

  const GfMatrix4d transform = simpleLight.GetTransform();
  GfVec3d          worldPos(position[0], position[1], position[2]);
  if(simpleLight.IsCameraSpaceLight())
  {
    worldPos = transform.Transform(worldPos);
  }

  // Keep the sphere-light approximation visible in the current shader attenuation model.
  constexpr float kSimpleLightIntensityScale = 1.0f;
  light.position = glm::vec3(static_cast<float>(worldPos[0]), static_cast<float>(worldPos[1]),
                             static_cast<float>(worldPos[2]));
  light.baseEmission = glm::vec3(diffuse[0] * kSimpleLightIntensityScale, diffuse[1] * kSimpleLightIntensityScale,
                                 diffuse[2] * kSimpleLightIntensityScale);
  light.diffuse      = diffuse[3];
  light.specular     = _MaxRgb(specular);
  light.radius       = 0.5f;

  _lightData = light;
  _EnqueueLightUpdate();
  *dirtyBits = HdChangeTracker::Clean;
}

// --------Distant Light-------------------------
HdRobotDistantLight::HdRobotDistantLight(const SdfPath& id, SceneStore& sceneStore, LightHandle handle)
    : HdRobotLight(id, sceneStore, handle)
{
}

void HdRobotDistantLight::Sync(HdSceneDelegate* sceneDelegate, [[maybe_unused]] HdRenderParam* renderParam,
                               HdDirtyBits* dirtyBits)
{
  if(!_CanSyncLight())
  {
    *dirtyBits = HdChangeTracker::Clean;
    return;
  }

  const SdfPath& id = GetId();
  HydraLight light = _lightData;
  light.type = 1;
  light.valid = 1;
  if (*dirtyBits & DirtyBits::DirtyTransform)
  {
    const GfMatrix4f transform(sceneDelegate->GetTransform(id));
    GfMatrix4f normalMatrix(transform.GetInverse().GetTranspose());

    GfVec3f dir = normalMatrix.TransformDir(GfVec3f(0.0f, 0.0f, -1.0f));
    // dir.Normalize();
    light.direction = glm::vec3(dir[0], dir[1], dir[2]);
  }

  if (*dirtyBits & DirtyBits::DirtyParams)
  {
    VtValue boxedAngle = sceneDelegate->GetLightParamValue(id, HdLightTokens->angle);
    float angle = GfDegreesToRadians(boxedAngle.GetWithDefault<float>(0.53f));

    VtValue boxedNormalize = sceneDelegate->GetLightParamValue(id, HdLightTokens->normalize);
    bool normalize = boxedNormalize.GetWithDefault<bool>(false);
    float sinHalfAngle = sinf(angle * 0.5f);
    float normalizeFactor = (sinHalfAngle > 1.0e-6f && normalize) ? (GfSqr(sinHalfAngle) * M_PI) : 1.0f;
    GfVec3f baseEmission = _CalcBaseEmission(sceneDelegate, normalizeFactor);

    VtValue boxedDiffuse = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse);
    float diffuse = boxedDiffuse.GetWithDefault<float>(1.0f);
    VtValue boxedSpecular = sceneDelegate->GetLightParamValue(id, HdLightTokens->specular);
    float specular = boxedSpecular.GetWithDefault<float>(1.0f);

    light.baseEmission = glm::vec3(baseEmission[0], baseEmission[1], baseEmission[2]);
    light.diffuse = diffuse;
    light.specular = specular;
    light.angle = angle;
  }

  _lightData = light;
  _EnqueueLightUpdate();
  *dirtyBits = HdChangeTracker::Clean;
}

// --------Dome Light-------------------------
HdRobotDomeLight::HdRobotDomeLight(const SdfPath& id, SceneStore& sceneStore, LightHandle handle)
    : HdRobotLight(id, sceneStore, handle)
{
}

std::string HdRobotDomeLight::GetTexturePath(HdSceneDelegate* sceneDelegate)
{
  const SdfPath& id = GetId();
  VtValue boxedTextureFile = sceneDelegate->GetLightParamValue(id, HdLightTokens->textureFile);
  if (boxedTextureFile.IsEmpty())
  {
    // Hydra runtime warns of empty path; we don't need to repeat it.
    return {};
  }

  if (!boxedTextureFile.IsHolding<SdfAssetPath>())
  {
    TF_WARN("%s:%s does not hold SdfAssetPath - ignoring!", id.GetText(), HdLightTokens->textureFile.GetText());
    return {};
  }

  const SdfAssetPath& assetPath = boxedTextureFile.UncheckedGet<SdfAssetPath>();

  std::string path = assetPath.GetResolvedPath();
  if (path.empty())
  {
    // Texture file is missing
    TF_WARN("Unable to resolve asset path \"%s\"", assetPath.GetAssetPath().c_str());
    return {};
  }
  return path;
}

void HdRobotDomeLight::Sync(HdSceneDelegate* sceneDelegate, [[maybe_unused]] HdRenderParam* renderParam,
                            HdDirtyBits* dirtyBits)
{
  if(!_CanSyncLight())
  {
    *dirtyBits = HdChangeTracker::Clean;
    return;
  }

  const SdfPath& id = GetId();
  HydraLight light = _lightData;
  light.type = 2;
  light.valid = 1;
  if (*dirtyBits & DirtyBits::DirtyTransform)
  {
    const GfMatrix4d& transform = sceneDelegate->GetTransform(id);
    auto rotateQuat = GfMatrix4f(transform.GetOrthonormalized()).ExtractRotationQuat();
    light.rotateQuat = glm::vec4(rotateQuat.GetImaginary()[0], rotateQuat.GetImaginary()[1],
                                 rotateQuat.GetImaginary()[2], rotateQuat.GetReal());
  }

  if (*dirtyBits & DirtyBits::DirtyParams)
  {
    GfVec3f baseEmission = _CalcBaseEmission(sceneDelegate);
    VtValue boxedDiffuse = sceneDelegate->GetLightParamValue(id, HdLightTokens->diffuse);
    float diffuse = boxedDiffuse.GetWithDefault<float>(1.0f);
    VtValue boxedSpecular = sceneDelegate->GetLightParamValue(id, HdLightTokens->specular);
    float specular = boxedSpecular.GetWithDefault<float>(1.0f);

    light.baseEmission = glm::vec3(baseEmission[0], baseEmission[1], baseEmission[2]);
    light.diffuse = diffuse;
    light.specular = specular;
    std::string texturePath = GetTexturePath(sceneDelegate);
    if(texturePath.empty())
    {
      light.texturePath.clear();
      light.textureID = -1;
    }
    else if (light.texturePath != texturePath)
    {
      light.texturePath = texturePath;
      light.textureID = _sceneStore.RegisterTexturePath(texturePath, TextureUsage::Light);
    }
  }

  _lightData = light;
  _EnqueueLightUpdate();
  *dirtyBits = HdChangeTracker::Clean;
}

PXR_NAMESPACE_CLOSE_SCOPE
