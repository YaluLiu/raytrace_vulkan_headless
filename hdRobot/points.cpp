#include "points.h"

#include "pxr/base/gf/matrix4d.h"
#include "pxr/imaging/hd/repr.h"
#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hd/tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

HdRobotPoints::HdRobotPoints(const SdfPath& id, HdRobotRenderParam& scene)
    : HdPoints(id)
    , _scene(scene)
    , _visible(true)
{
}

HdRobotPoints::~HdRobotPoints() = default;

HdDirtyBits HdRobotPoints::GetInitialDirtyBitsMask() const
{
  return HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyWidths | HdChangeTracker::DirtyPrimvar
         | HdChangeTracker::DirtyTransform | HdChangeTracker::DirtyVisibility | HdChangeTracker::DirtyRepr;
}

void HdRobotPoints::_InitRepr(TfToken const& reprToken, HdDirtyBits* dirtyBits)
{
  TF_UNUSED(reprToken);
  *dirtyBits |= HdChangeTracker::DirtyRepr;
}

HdDirtyBits HdRobotPoints::_PropagateDirtyBits(HdDirtyBits bits) const
{
  return bits;
}

void HdRobotPoints::Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits, TfToken const& reprToken)
{
  TF_UNUSED(renderParam);
  TF_UNUSED(reprToken);

  if(*dirtyBits & (HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyWidths | HdChangeTracker::DirtyPrimvar))
  {
    _UpdateGeometry(sceneDelegate, dirtyBits);
  }

  if(*dirtyBits & HdChangeTracker::DirtyTransform)
  {
    _UpdateTransform(sceneDelegate, dirtyBits);
  }

  if(*dirtyBits & HdChangeTracker::DirtyVisibility)
  {
    _UpdateVisibility(sceneDelegate, dirtyBits);
  }

  *dirtyBits = HdChangeTracker::Clean;
}

void HdRobotPoints::_UpdateGeometry(HdSceneDelegate* sceneDelegate, HdDirtyBits* dirtyBits)
{
  const SdfPath& id = GetId();

  if(*dirtyBits & HdChangeTracker::DirtyPoints)
  {
    VtValue pointsValue = sceneDelegate->Get(id, HdTokens->points);
    if(pointsValue.IsHolding<VtVec3fArray>())
    {
      _points = pointsValue.UncheckedGet<VtVec3fArray>();
    }
  }

  if(*dirtyBits & HdChangeTracker::DirtyWidths)
  {
    VtValue widthsValue = sceneDelegate->Get(id, HdTokens->widths);
    if(widthsValue.IsHolding<VtFloatArray>())
    {
      _widths = widthsValue.UncheckedGet<VtFloatArray>();
    }
    else
    {
      _widths.assign(_points.size(), 1.0f);
    }
  }

  if(*dirtyBits & HdChangeTracker::DirtyPrimvar)
  {
    VtValue colorValue = sceneDelegate->Get(id, HdTokens->displayColor);
    if(colorValue.IsHolding<VtVec3fArray>())
    {
      _colors = colorValue.UncheckedGet<VtVec3fArray>();
    }
    else
    {
      _colors.assign(_points.size(), GfVec3f(1.0f, 1.0f, 1.0f));
    }
  }

  if(*dirtyBits & HdChangeTracker::DirtyPoints)
  {
    _UpdateRenderScene();
  }
}

void HdRobotPoints::_UpdateTransform(HdSceneDelegate* sceneDelegate, HdDirtyBits* dirtyBits)
{
  TF_UNUSED(dirtyBits);
  const SdfPath& id = GetId();
  _transform        = sceneDelegate->GetTransform(id);
}

void HdRobotPoints::_UpdateVisibility(HdSceneDelegate* sceneDelegate, HdDirtyBits* dirtyBits)
{
  TF_UNUSED(dirtyBits);
  const SdfPath& id = GetId();
  _visible          = sceneDelegate->GetVisible(id);
}

void HdRobotPoints::_UpdateRenderScene() const
{
  // The minimal raster path does not draw HdPoints yet.
}

PXR_NAMESPACE_CLOSE_SCOPE
