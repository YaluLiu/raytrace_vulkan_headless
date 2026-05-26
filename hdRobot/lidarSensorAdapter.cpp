#include "lidarSensorAdapter.h"

#include "tokens.h"

#include <pxr/base/tf/registryManager.h>
#include <pxr/base/tf/stringUtils.h>
#include <pxr/imaging/hd/perfLog.h>
#include <pxr/imaging/hd/sprim.h>
#include <pxr/usd/usd/attribute.h>
#include <pxr/usd/usd/inherits.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformable.h>
#include <pxr/usdImaging/usdImaging/indexProxy.h>
#include <pxr/usdImaging/usdImaging/tokens.h>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
constexpr HdDirtyBits kDirtyTransform = 1 << 0;
constexpr HdDirtyBits kDirtyParams = 1 << 1;

bool IsLidarProperty(const TfToken& propertyName)
{
  return TfStringStartsWith(propertyName.GetString(), "lidar:");
}

bool IsXformProperty(const TfToken& propertyName)
{
  return UsdGeomXformable::IsTransformationAffectedByAttrNamed(propertyName);
}

} // namespace

TF_REGISTRY_FUNCTION(TfType)
{
  using Adapter = HdRobotLidarSensorAdapter;
  TfType t = TfType::Define<Adapter, TfType::Bases<Adapter::BaseAdapter>>();
  t.SetFactory<UsdImagingPrimAdapterFactory<Adapter>>();
}

HdRobotLidarSensorAdapter::~HdRobotLidarSensorAdapter() = default;

TfTokenVector HdRobotLidarSensorAdapter::GetImagingSubprims(UsdPrim const&)
{
  return {TfToken()};
}

TfToken HdRobotLidarSensorAdapter::GetImagingSubprimType(UsdPrim const&, TfToken const& subprim)
{
  return subprim.IsEmpty() ? HdRobotPrimTypeTokens->lidarSensor : TfToken();
}

bool HdRobotLidarSensorAdapter::IsSupported(UsdImagingIndexProxy const* index) const
{
  return index != nullptr && index->IsSprimTypeSupported(HdRobotPrimTypeTokens->lidarSensor);
}

SdfPath HdRobotLidarSensorAdapter::Populate(UsdPrim const& prim,
                                            UsdImagingIndexProxy* index,
                                            UsdImagingInstancerContext const* instancerContext)
{
  if(index == nullptr)
  {
    return SdfPath();
  }

  const SdfPath cachePath = ResolveCachePath(prim.GetPath(), instancerContext);
  index->InsertSprim(HdRobotPrimTypeTokens->lidarSensor, cachePath, prim);

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

void HdRobotLidarSensorAdapter::TrackVariability(UsdPrim const& prim,
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

void HdRobotLidarSensorAdapter::UpdateForTime(UsdPrim const&,
                                              SdfPath const&,
                                              UsdTimeCode,
                                              HdDirtyBits,
                                              UsdImagingInstancerContext const*) const
{
}

HdDirtyBits HdRobotLidarSensorAdapter::ProcessPropertyChange(UsdPrim const&,
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

void HdRobotLidarSensorAdapter::MarkDirty(UsdPrim const&,
                                          SdfPath const& cachePath,
                                          HdDirtyBits dirty,
                                          UsdImagingIndexProxy* index)
{
  if(index != nullptr)
  {
    index->MarkSprimDirty(cachePath, dirty);
  }
}

VtValue HdRobotLidarSensorAdapter::Get(UsdPrim const& prim,
                                       SdfPath const& cachePath,
                                       TfToken const& key,
                                       UsdTimeCode time,
                                       VtIntArray* outIndices) const
{
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

bool HdRobotLidarSensorAdapter::GetVisible(UsdPrim const&,
                                           SdfPath const&,
                                           UsdTimeCode) const
{
  return true;
}

TfToken HdRobotLidarSensorAdapter::GetPurpose(UsdPrim const&,
                                              SdfPath const&,
                                              TfToken const&) const
{
  return UsdGeomTokens->default_;
}

GfMatrix4d HdRobotLidarSensorAdapter::GetTransform(UsdPrim const& prim,
                                                   SdfPath const& cachePath,
                                                   UsdTimeCode time,
                                                   bool ignoreRootTransform) const
{
  return BaseAdapter::GetTransform(prim, cachePath, time, ignoreRootTransform);
}

void HdRobotLidarSensorAdapter::_RemovePrim(SdfPath const& cachePath,
                                            UsdImagingIndexProxy* index)
{
  if(index != nullptr)
  {
    index->RemoveSprim(HdRobotPrimTypeTokens->lidarSensor, cachePath);
  }
}

PXR_NAMESPACE_CLOSE_SCOPE
