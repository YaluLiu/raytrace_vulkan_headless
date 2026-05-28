#include "heightScanSensorAdapter.h"

#include "hydraSensor.h"
#include "../UsdRaySensor/heightScanSensor.h"
#include "../UsdRaySensor/tokens.h"

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
bool IsHeightScanProperty(const TfToken& propertyName)
{
  const TfTokenVector& names = UsdGeomHeightScanSensor::GetSchemaAttributeNames(false);
  return std::find(names.begin(), names.end(), propertyName) != names.end();
}

bool IsXformProperty(const TfToken& propertyName)
{
  return UsdGeomXformable::IsTransformationAffectedByAttrNamed(propertyName);
}

} // namespace

TF_REGISTRY_FUNCTION(TfType)
{
  using Adapter = HeightScanSensorAdapter;
  TfType t = TfType::Define<Adapter, TfType::Bases<Adapter::BaseAdapter>>();
  t.SetFactory<UsdImagingPrimAdapterFactory<Adapter>>();
}

HeightScanSensorAdapter::~HeightScanSensorAdapter() = default;

TfTokenVector HeightScanSensorAdapter::GetImagingSubprims(UsdPrim const&)
{
  return {TfToken()};
}

TfToken HeightScanSensorAdapter::GetImagingSubprimType(UsdPrim const&, TfToken const& subprim)
{
  return subprim.IsEmpty() ? HdRaySensorPrimTypeTokens->heightScanSensor : TfToken();
}

bool HeightScanSensorAdapter::IsSupported(UsdImagingIndexProxy const* index) const
{
  return index != nullptr && index->IsSprimTypeSupported(HdRaySensorPrimTypeTokens->heightScanSensor);
}

SdfPath HeightScanSensorAdapter::Populate(UsdPrim const& prim,
                                                 UsdImagingIndexProxy* index,
                                                 UsdImagingInstancerContext const* instancerContext)
{
  if(index == nullptr)
  {
    return SdfPath();
  }

  const SdfPath cachePath = ResolveCachePath(prim.GetPath(), instancerContext);
  index->InsertSprim(HdRaySensorPrimTypeTokens->heightScanSensor, cachePath, prim);

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

void HeightScanSensorAdapter::TrackVariability(UsdPrim const& prim,
                                                      SdfPath const&,
                                                      HdDirtyBits* timeVaryingBits,
                                                      UsdImagingInstancerContext const*) const
{
  if(timeVaryingBits == nullptr)
  {
    return;
  }

  _IsTransformVarying(prim,
                      HdRaySensorDirtyBits::DirtyTransform,
                      UsdImagingTokens->usdVaryingXform,
                      timeVaryingBits);

  for(const UsdAttribute& attr : prim.GetAttributes())
  {
    if(IsHeightScanProperty(attr.GetName()) && attr.ValueMightBeTimeVarying())
    {
      *timeVaryingBits |= HdRaySensorDirtyBits::DirtyParams;
      break;
    }
  }
}

void HeightScanSensorAdapter::UpdateForTime(UsdPrim const&,
                                                   SdfPath const&,
                                                   UsdTimeCode,
                                                   HdDirtyBits,
                                                   UsdImagingInstancerContext const*) const
{
}

HdDirtyBits HeightScanSensorAdapter::ProcessPropertyChange(UsdPrim const&,
                                                                  SdfPath const&,
                                                                  TfToken const& propertyName)
{
  if(IsXformProperty(propertyName))
  {
    return HdRaySensorDirtyBits::DirtyTransform;
  }
  if(IsHeightScanProperty(propertyName))
  {
    return HdRaySensorDirtyBits::DirtyParams;
  }
  return HdRaySensorDirtyBits::DirtyParams;
}

void HeightScanSensorAdapter::MarkDirty(UsdPrim const&,
                                               SdfPath const& cachePath,
                                               HdDirtyBits dirty,
                                               UsdImagingIndexProxy* index)
{
  if(index != nullptr)
  {
    index->MarkSprimDirty(cachePath, dirty);
  }
}

void HeightScanSensorAdapter::MarkTransformDirty(UsdPrim const&,
                                                 SdfPath const& cachePath,
                                                 UsdImagingIndexProxy* index)
{
  if(index != nullptr)
  {
    index->MarkSprimDirty(cachePath, HdRaySensorDirtyBits::DirtyTransform);
  }
}

VtValue HeightScanSensorAdapter::Get(UsdPrim const& prim,
                                            SdfPath const& cachePath,
                                            TfToken const& key,
                                            UsdTimeCode time,
                                            VtIntArray* outIndices) const
{
  const UsdGeomHeightScanSensor sensor(prim);
  if(key == UsdRaySensorTokens->enabled)
  {
    return VtValue(sensor.GetEnabled(time));
  }
  if(key == UsdRaySensorTokens->uStart)
  {
    return VtValue(sensor.GetUStart(time));
  }
  if(key == UsdRaySensorTokens->uEnd)
  {
    return VtValue(sensor.GetUEnd(time));
  }
  if(key == UsdRaySensorTokens->uStep)
  {
    return VtValue(sensor.GetUStep(time));
  }
  if(key == UsdRaySensorTokens->vStart)
  {
    return VtValue(sensor.GetVStart(time));
  }
  if(key == UsdRaySensorTokens->vEnd)
  {
    return VtValue(sensor.GetVEnd(time));
  }
  if(key == UsdRaySensorTokens->vStep)
  {
    return VtValue(sensor.GetVStep(time));
  }
  if(key == UsdRaySensorTokens->gravityDirectionWs)
  {
    return VtValue(sensor.GetGravityDirectionWs(time));
  }
  if(key == UsdRaySensorTokens->maxRange)
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

bool HeightScanSensorAdapter::GetVisible(UsdPrim const&,
                                                SdfPath const&,
                                                UsdTimeCode) const
{
  return true;
}

TfToken HeightScanSensorAdapter::GetPurpose(UsdPrim const&,
                                                   SdfPath const&,
                                                   TfToken const&) const
{
  return UsdGeomTokens->default_;
}

GfMatrix4d HeightScanSensorAdapter::GetTransform(UsdPrim const& prim,
                                                        SdfPath const& cachePath,
                                                        UsdTimeCode time,
                                                        bool ignoreRootTransform) const
{
  return BaseAdapter::GetTransform(prim, cachePath, time, ignoreRootTransform);
}

void HeightScanSensorAdapter::_RemovePrim(SdfPath const& cachePath,
                                                 UsdImagingIndexProxy* index)
{
  if(index != nullptr)
  {
    index->RemoveSprim(HdRaySensorPrimTypeTokens->heightScanSensor, cachePath);
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
