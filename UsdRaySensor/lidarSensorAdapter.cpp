#include "lidarSensorAdapter.h"

#include "lidarSensor.h"
#include "tokens.h"

#include <pxr/base/tf/registryManager.h>
#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/perfLog.h>
#include <pxr/imaging/hd/sprim.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/inherits.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usdImaging/usdImaging/indexProxy.h>
#include <pxr/usdImaging/usdImaging/tokens.h>

#include <algorithm>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
constexpr HdDirtyBits kDirtyTransform = 1 << 0;
constexpr HdDirtyBits kDirtyParams = 1 << 1;

const TfToken& GetHydraLidarSensorType()
{
  static const TfToken token("lidarSensor");
  return token;
}

bool IsLidarProperty(const TfToken& propertyName)
{
  const TfTokenVector& names = UsdRaySensorLidarSensor::GetSchemaAttributeNames(false);
  return std::find(names.begin(), names.end(), propertyName) != names.end();
}

bool IsXformProperty(const TfToken& propertyName)
{
  return UsdGeomXformable::IsTransformationAffectedByAttrNamed(propertyName);
}

} // namespace

TF_REGISTRY_FUNCTION(TfType)
{
  using Adapter = UsdRaySensorLidarSensorAdapter;
  TfType t = TfType::Define<Adapter, TfType::Bases<Adapter::BaseAdapter>>();
  t.SetFactory<UsdImagingPrimAdapterFactory<Adapter>>();
}

UsdRaySensorLidarSensorAdapter::~UsdRaySensorLidarSensorAdapter() = default;

TfTokenVector UsdRaySensorLidarSensorAdapter::GetImagingSubprims(UsdPrim const&)
{
  return {TfToken()};
}

TfToken UsdRaySensorLidarSensorAdapter::GetImagingSubprimType(UsdPrim const&, TfToken const& subprim)
{
  return subprim.IsEmpty() ? GetHydraLidarSensorType() : TfToken();
}

bool UsdRaySensorLidarSensorAdapter::IsSupported(UsdImagingIndexProxy const* index) const
{
  return index != nullptr && index->IsSprimTypeSupported(GetHydraLidarSensorType());
}

SdfPath UsdRaySensorLidarSensorAdapter::Populate(UsdPrim const& prim,
                                            UsdImagingIndexProxy* index,
                                            UsdImagingInstancerContext const* instancerContext)
{
  if(index == nullptr)
  {
    return SdfPath();
  }

  const SdfPath cachePath = ResolveCachePath(prim.GetPath(), instancerContext);
  index->InsertSprim(GetHydraLidarSensorType(), cachePath, prim);

  for(const SdfPath& inheritPath : prim.GetInherits().GetAllDirectInherits())
  {
    if(UsdPrim inheritedPrim = prim.GetStage()->GetPrimAtPath(inheritPath))
    {
      index->AddDependency(cachePath, inheritedPrim);
    }
  }

  HD_PERF_COUNTER_INCR(UsdImagingTokens->usdPopulatedPrimCount);
  return cachePath;
}

void UsdRaySensorLidarSensorAdapter::TrackVariability(UsdPrim const& prim,
                                                 SdfPath const&,
                                                 HdDirtyBits* timeVaryingBits,
                                                 UsdImagingInstancerContext const*) const
{
  if(timeVaryingBits == nullptr)
  {
    return;
  }

  const UsdGeomXformable xformable(prim);
  if(xformable && xformable.TransformMightBeTimeVarying())
  {
    *timeVaryingBits |= kDirtyTransform;
  }

  for(const UsdAttribute& attr : prim.GetAttributes())
  {
    if(IsLidarProperty(attr.GetName()) && attr.ValueMightBeTimeVarying())
    {
      *timeVaryingBits |= kDirtyParams;
      break;
    }
  }
}

void UsdRaySensorLidarSensorAdapter::UpdateForTime(UsdPrim const&,
                                              SdfPath const&,
                                              UsdTimeCode,
                                              HdDirtyBits,
                                              UsdImagingInstancerContext const*) const
{
}

HdDirtyBits UsdRaySensorLidarSensorAdapter::ProcessPropertyChange(UsdPrim const&,
                                                             SdfPath const&,
                                                             TfToken const& propertyName)
{
  if(IsXformProperty(propertyName))
  {
    return kDirtyTransform;
  }
  if(IsLidarProperty(propertyName))
  {
    return kDirtyParams;
  }
  return kDirtyParams;
}

void UsdRaySensorLidarSensorAdapter::MarkDirty(UsdPrim const&,
                                          SdfPath const& cachePath,
                                          HdDirtyBits dirty,
                                          UsdImagingIndexProxy* index)
{
  if(index != nullptr)
  {
    index->MarkSprimDirty(cachePath, dirty);
  }
}

VtValue UsdRaySensorLidarSensorAdapter::Get(UsdPrim const& prim,
                                       SdfPath const& cachePath,
                                       TfToken const& key,
                                       UsdTimeCode time,
                                       VtIntArray* outIndices) const
{
  const UsdRaySensorLidarSensor sensor(prim);
  if(key == UsdRaySensorTokens->lidarSensorParams)
  {
    return VtValue(sensor.GetParams(time));
  }
  if(key == UsdRaySensorTokens->enabled)
  {
    return VtValue(sensor.GetEnabled(time));
  }
  if(key == UsdRaySensorTokens->azimuthStartDeg)
  {
    return VtValue(sensor.GetAzimuthStartDeg(time));
  }
  if(key == UsdRaySensorTokens->azimuthEndDeg)
  {
    return VtValue(sensor.GetAzimuthEndDeg(time));
  }
  if(key == UsdRaySensorTokens->azimuthStepDeg)
  {
    return VtValue(sensor.GetAzimuthStepDeg(time));
  }
  if(key == UsdRaySensorTokens->verticalStartDeg)
  {
    return VtValue(sensor.GetVerticalStartDeg(time));
  }
  if(key == UsdRaySensorTokens->verticalEndDeg)
  {
    return VtValue(sensor.GetVerticalEndDeg(time));
  }
  if(key == UsdRaySensorTokens->verticalStepDeg)
  {
    return VtValue(sensor.GetVerticalStepDeg(time));
  }
  if(key == UsdRaySensorTokens->maxRange)
  {
    return VtValue(sensor.GetMaxRange(time));
  }
  if(key == UsdRaySensorTokens->intensity)
  {
    return VtValue(sensor.GetIntensity(time));
  }

  if(UsdAttribute attr = prim.GetAttribute(key))
  {
    VtValue value;
    if(attr.Get(&value, time))
    {
      return value;
    }
  }
  if(key == UsdGeomTokens->purpose)
  {
    return VtValue(UsdGeomTokens->default_);
  }
  if(key == UsdGeomTokens->visibility)
  {
    return VtValue(UsdGeomTokens->inherited);
  }
  return BaseAdapter::Get(prim, cachePath, key, time, outIndices);
}

bool UsdRaySensorLidarSensorAdapter::GetVisible(UsdPrim const&,
                                           SdfPath const&,
                                           UsdTimeCode) const
{
  return true;
}

TfToken UsdRaySensorLidarSensorAdapter::GetPurpose(UsdPrim const&,
                                              SdfPath const&,
                                              TfToken const&) const
{
  return UsdGeomTokens->default_;
}

GfMatrix4d UsdRaySensorLidarSensorAdapter::GetTransform(UsdPrim const& prim,
                                                   SdfPath const& cachePath,
                                                   UsdTimeCode time,
                                                   bool ignoreRootTransform) const
{
  return BaseAdapter::GetTransform(prim, cachePath, time, ignoreRootTransform);
}

void UsdRaySensorLidarSensorAdapter::_RemovePrim(SdfPath const& cachePath,
                                            UsdImagingIndexProxy* index)
{
  if(index != nullptr)
  {
    index->RemoveSprim(GetHydraLidarSensorType(), cachePath);
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
