#pragma once

#include <pxr/usdImaging/usdImaging/primAdapter.h>

PXR_NAMESPACE_OPEN_SCOPE

class HeightScanSensorAdapter final : public UsdImagingPrimAdapter
{
public:
  using BaseAdapter = UsdImagingPrimAdapter;

  HeightScanSensorAdapter() = default;
  ~HeightScanSensorAdapter() override;

  TfTokenVector GetImagingSubprims(UsdPrim const& prim) override;
  TfToken GetImagingSubprimType(UsdPrim const& prim, TfToken const& subprim) override;
  bool IsSupported(UsdImagingIndexProxy const* index) const override;

  SdfPath Populate(UsdPrim const& prim,
                   UsdImagingIndexProxy* index,
                   UsdImagingInstancerContext const* instancerContext = nullptr) override;

  void TrackVariability(UsdPrim const& prim,
                        SdfPath const& cachePath,
                        HdDirtyBits* timeVaryingBits,
                        UsdImagingInstancerContext const* instancerContext = nullptr) const override;

  void UpdateForTime(UsdPrim const& prim,
                     SdfPath const& cachePath,
                     UsdTimeCode time,
                     HdDirtyBits requestedBits,
                     UsdImagingInstancerContext const* instancerContext = nullptr) const override;

  HdDirtyBits ProcessPropertyChange(UsdPrim const& prim,
                                    SdfPath const& cachePath,
                                    TfToken const& propertyName) override;

  void MarkDirty(UsdPrim const& prim,
                 SdfPath const& cachePath,
                 HdDirtyBits dirty,
                 UsdImagingIndexProxy* index) override;

  void MarkTransformDirty(UsdPrim const& prim,
                          SdfPath const& cachePath,
                          UsdImagingIndexProxy* index) override;

  VtValue Get(UsdPrim const& prim,
              SdfPath const& cachePath,
              TfToken const& key,
              UsdTimeCode time,
              VtIntArray* outIndices) const override;

  bool GetVisible(UsdPrim const& prim,
                  SdfPath const& cachePath,
                  UsdTimeCode time) const override;

  TfToken GetPurpose(UsdPrim const& prim,
                     SdfPath const& cachePath,
                     TfToken const& instanceInheritablePurpose) const override;

  GfMatrix4d GetTransform(UsdPrim const& prim,
                          SdfPath const& cachePath,
                          UsdTimeCode time,
                          bool ignoreRootTransform = false) const override;

protected:
  void _RemovePrim(SdfPath const& cachePath,
                   UsdImagingIndexProxy* index) override;
};

PXR_NAMESPACE_CLOSE_SCOPE
