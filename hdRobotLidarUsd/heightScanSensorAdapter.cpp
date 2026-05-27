#include "heightScanSensorAdapter.h"

#include "heightScanSensor.h"
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

const TfToken& GetHydraHeightScanSensorType()
{
  static const TfToken token("heightScanSensor");
  return token;
}

bool IsHeightScanProperty(const TfToken& propertyName)
{
  const TfTokenVector& names = HdRobotLidarUsdHeightScanSensor::GetSchemaAttributeNames(false);
  return std::find(names.begin(), names.end(), propertyName) != names.end();
}

bool IsXformProperty(const TfToken& propertyName)
{
  return UsdGeomXformable::IsTransformationAffectedByAttrNamed(propertyName);
}

} // namespace

TF_REGISTRY_FUNCTION(TfType)
{
  using Adapter = HdRobotHeightScanSensorAdapter;
  TfType t = TfType::Define<Adapter, TfType::Bases<Adapter::BaseAdapter>>();
  t.SetFactory<UsdImagingPrimAdapterFactory<Adapter>>();
}

HdRobotHeightScanSensorAdapter::~HdRobotHeightScanSensorAdapter() = default;

TfTokenVector HdRobotHeightScanSensorAdapter::GetImagingSubprims(UsdPrim const&)
{
  return {TfToken()};
}

TfToken HdRobotHeightScanSensorAdapter::GetImagingSubprimType(UsdPrim const&, TfToken const& subprim)
{
  return subprim.IsEmpty() ? GetHydraHeightScanSensorType() : TfToken();
}

bool HdRobotHeightScanSensorAdapter::IsSupported(UsdImagingIndexProxy const* index) const
{
  return index != nullptr && index->IsSprimTypeSupported(GetHydraHeightScanSensorType());
}

SdfPath HdRobotHeightScanSensorAdapter::Populate(UsdPrim const& prim,
                                                 UsdImagingIndexProxy* index,
                                                 UsdImagingInstancerContext const* instancerContext)
{
  if(index == nullptr)
  {
    return SdfPath();
  }

  const SdfPath cachePath = ResolveCachePath(prim.GetPath(), instancerContext);
  index->InsertSprim(GetHydraHeightScanSensorType(), cachePath, prim);

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

void HdRobotHeightScanSensorAdapter::TrackVariability(UsdPrim const& prim,
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
    if(IsHeightScanProperty(attr.GetName()) && attr.ValueMightBeTimeVarying())
    {
      *timeVaryingBits |= kDirtyParams;
      break;
    }
  }
}

void HdRobotHeightScanSensorAdapter::UpdateForTime(UsdPrim const&,
                                                   SdfPath const&,
                                                   UsdTimeCode,
                                                   HdDirtyBits,
                                                   UsdImagingInstancerContext const*) const
{
}

HdDirtyBits HdRobotHeightScanSensorAdapter::ProcessPropertyChange(UsdPrim const&,
                                                                  SdfPath const&,
                                                                  TfToken const& propertyName)
{
  if(IsXformProperty(propertyName))
  {
    return kDirtyTransform;
  }
  if(IsHeightScanProperty(propertyName))
  {
    return kDirtyParams;
  }
  return kDirtyParams;
}

void HdRobotHeightScanSensorAdapter::MarkDirty(UsdPrim const&,
                                               SdfPath const& cachePath,
                                               HdDirtyBits dirty,
                                               UsdImagingIndexProxy* index)
{
  if(index != nullptr)
  {
    index->MarkSprimDirty(cachePath, dirty);
  }
}

VtValue HdRobotHeightScanSensorAdapter::Get(UsdPrim const& prim,
                                            SdfPath const& cachePath,
                                            TfToken const& key,
                                            UsdTimeCode time,
                                            VtIntArray* outIndices) const
{
  const HdRobotLidarUsdHeightScanSensor sensor(prim);
  if(key == HdRobotLidarUsdTokens->heightScanSensorParams)
  {
    return VtValue(sensor.GetParams(time));
  }
  if(key == HdRobotLidarUsdTokens->enabled)
  {
    return VtValue(sensor.GetEnabled(time));
  }
  if(key == HdRobotLidarUsdTokens->uStart)
  {
    return VtValue(sensor.GetUStart(time));
  }
  if(key == HdRobotLidarUsdTokens->uEnd)
  {
    return VtValue(sensor.GetUEnd(time));
  }
  if(key == HdRobotLidarUsdTokens->uStep)
  {
    return VtValue(sensor.GetUStep(time));
  }
  if(key == HdRobotLidarUsdTokens->vStart)
  {
    return VtValue(sensor.GetVStart(time));
  }
  if(key == HdRobotLidarUsdTokens->vEnd)
  {
    return VtValue(sensor.GetVEnd(time));
  }
  if(key == HdRobotLidarUsdTokens->vStep)
  {
    return VtValue(sensor.GetVStep(time));
  }
  if(key == HdRobotLidarUsdTokens->rayDirection)
  {
    return VtValue(sensor.GetRayDirection(time));
  }
  if(key == HdRobotLidarUsdTokens->maxRange)
  {
    return VtValue(sensor.GetMaxRange(time));
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

bool HdRobotHeightScanSensorAdapter::GetVisible(UsdPrim const&,
                                                SdfPath const&,
                                                UsdTimeCode) const
{
  return true;
}

TfToken HdRobotHeightScanSensorAdapter::GetPurpose(UsdPrim const&,
                                                   SdfPath const&,
                                                   TfToken const&) const
{
  return UsdGeomTokens->default_;
}

GfMatrix4d HdRobotHeightScanSensorAdapter::GetTransform(UsdPrim const& prim,
                                                        SdfPath const& cachePath,
                                                        UsdTimeCode time,
                                                        bool ignoreRootTransform) const
{
  return BaseAdapter::GetTransform(prim, cachePath, time, ignoreRootTransform);
}

void HdRobotHeightScanSensorAdapter::_RemovePrim(SdfPath const& cachePath,
                                                 UsdImagingIndexProxy* index)
{
  if(index != nullptr)
  {
    index->RemoveSprim(GetHydraHeightScanSensorType(), cachePath);
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
