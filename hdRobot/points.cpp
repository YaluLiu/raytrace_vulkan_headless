#include "points.h"
#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hd/repr.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/base/gf/matrix4d.h"
#include <iostream>
#include <iomanip>
#include <algorithm>

PXR_NAMESPACE_OPEN_SCOPE

HdRobotPoints::HdRobotPoints(const SdfPath& id, HdRobotRenderParam& scene)
    : HdPoints(id)
    , _scene(scene)
    , _visible(true)
{
}

HdRobotPoints::~HdRobotPoints() {}

HdDirtyBits HdRobotPoints::GetInitialDirtyBitsMask() const
{
  return HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyWidths | HdChangeTracker::DirtyPrimvar
         | HdChangeTracker::DirtyTransform | HdChangeTracker::DirtyVisibility | HdChangeTracker::DirtyRepr;
}

void HdRobotPoints::_InitRepr(TfToken const& reprToken, HdDirtyBits* dirtyBits)
{
  // 绠€鍗曠殑瀹炵幇 - 璁╁熀绫诲鐞唕epr鍒濆鍖?
  // 鍦ㄥぇ澶氭暟鎯呭喌涓嬶紝杩欏氨瓒冲浜?
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

  SdfPath const& id = GetId();

  // 鏇存柊鍑犱綍鏁版嵁
  if(*dirtyBits & (HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyWidths | HdChangeTracker::DirtyPrimvar))
  {
    _UpdateGeometry(sceneDelegate, dirtyBits);
  }

  // 鏇存柊鍙樻崲
  if(*dirtyBits & HdChangeTracker::DirtyTransform)
  {
    _UpdateTransform(sceneDelegate, dirtyBits);
  }

  // 鏇存柊鍙鎬?
  if(*dirtyBits & HdChangeTracker::DirtyVisibility)
  {
    _UpdateVisibility(sceneDelegate, dirtyBits);
  }

  // 娓呴櫎鑴忎綅
  *dirtyBits = HdChangeTracker::Clean;
}

void HdRobotPoints::_UpdateGeometry(HdSceneDelegate* sceneDelegate, HdDirtyBits* dirtyBits)
{
  SdfPath const& id = GetId();

  // 璇诲彇鐐逛綅缃紙蹇呴渶锛?
  if(*dirtyBits & HdChangeTracker::DirtyPoints)
  {
    VtValue pointsValue = sceneDelegate->Get(id, HdTokens->points);
    if(pointsValue.IsHolding<VtVec3fArray>())
    {
      _points = pointsValue.UncheckedGet<VtVec3fArray>();
    }
  }

  // 璇诲彇鐐瑰搴︼紙蹇呴渶锛?
  if(*dirtyBits & HdChangeTracker::DirtyWidths)
  {
    VtValue widthsValue = sceneDelegate->Get(id, HdTokens->widths);
    if(widthsValue.IsHolding<VtFloatArray>())
    {
      _widths = widthsValue.UncheckedGet<VtFloatArray>();
    }
    else
    {
      // 濡傛灉娌℃湁瀹藉害鏁版嵁锛屼娇鐢ㄩ粯璁ゅ€?
      _widths.assign(_points.size(), 1.0f);
    }
  }

  // 璇诲彇棰滆壊锛堝彲閫夛級
  if(*dirtyBits & HdChangeTracker::DirtyPrimvar)
  {
    // 灏濊瘯鑾峰彇displayColor
    VtValue colorValue = sceneDelegate->Get(id, HdTokens->displayColor);
    if(colorValue.IsHolding<VtVec3fArray>())
    {
      _colors = colorValue.UncheckedGet<VtVec3fArray>();
    }
    else
    {
      // 浣跨敤榛樿鐧借壊
      _colors.assign(_points.size(), GfVec3f(1.0f, 1.0f, 1.0f));
    }
  }

  // 鎵撳嵃鐐规暟鎹?
  if(*dirtyBits & HdChangeTracker::DirtyPoints)
  {
    _UpdateRenderScene();
    // _PrintPointsDetailed();
  }
}

void HdRobotPoints::_UpdateTransform(HdSceneDelegate* sceneDelegate, HdDirtyBits* dirtyBits)
{
  TF_UNUSED(dirtyBits);
  SdfPath const& id = GetId();
  _transform        = sceneDelegate->GetTransform(id);
}

void HdRobotPoints::_UpdateVisibility(HdSceneDelegate* sceneDelegate, HdDirtyBits* dirtyBits)
{
  TF_UNUSED(dirtyBits);
  SdfPath const& id = GetId();
  _visible          = sceneDelegate->GetVisible(id);
}

void HdRobotPoints::_UpdateRenderScene() const
{
  const size_t pointsSize = _points.size();
  _scene.v_sphere.resize(pointsSize);
  for(size_t i = 0; i < pointsSize; ++i)
  {
    const GfVec3f& point      = _points[i];
    _scene.v_sphere[i].center = glm::vec3(point[0], point[1], point[2]);
    _scene.v_sphere[i].radius = i < _widths.size() ? _widths[i] : 1.0f;
  }
}

// 鎵撳嵃鍑芥暟瀹炵幇
void HdRobotPoints::_PrintPoints() const
{
  std::cout << "=== Points Data ===" << std::endl;
  std::cout << "Total points: " << _points.size() << std::endl;

  for(size_t i = 0; i < _points.size(); ++i)
  {
    const GfVec3f& point = _points[i];
    std::cout << "Point[" << i << "]: (" << point[0] << ", " << point[1] << ", " << point[2] << ")" << std::endl;
  }
  std::cout << "===================" << std::endl;
}

void HdRobotPoints::_PrintPointsDetailed() const
{
  std::cout << "=== Detailed Points Data ===" << std::endl;
  std::cout << "Total points: " << _points.size() << std::endl;
  std::cout << "Widths count: " << _widths.size() << std::endl;
  std::cout << "Colors count: " << _colors.size() << std::endl;
  std::cout << std::fixed << std::setprecision(3);

  const size_t sampleCount = std::min<size_t>(20, _points.size());
  for(size_t i = 0; i < sampleCount; ++i)
  {
    const GfVec3f& point = _points[i];

    std::cout << "Point[" << std::setw(4) << i << "]: "
              << "pos(" << std::setw(8) << point[0] << ", " << std::setw(8) << point[1] << ", " << std::setw(8)
              << point[2] << ")";

    // 鎵撳嵃瀹藉害淇℃伅
    if(i < _widths.size())
    {
      std::cout << " width(" << std::setw(6) << _widths[i] << ")";
    }

    // 鎵撳嵃棰滆壊淇℃伅
    if(i < _colors.size())
    {
      const GfVec3f& color = _colors[i];
      std::cout << " color(" << std::setw(5) << color[0] << ", " << std::setw(5) << color[1] << ", " << std::setw(5)
                << color[2] << ")";
    }

    std::cout << std::endl;
  }
  std::cout << "=============================" << std::endl;
}

void HdRobotPoints::_PrintPointsSummary() const
{
  std::cout << "=== Points Summary ===" << std::endl;
  std::cout << "Total points: " << _points.size() << std::endl;

  if(_points.empty())
  {
    std::cout << "No points data available." << std::endl;
    std::cout << "======================" << std::endl;
    return;
  }

  // 璁＄畻杈圭晫妗?
  GfVec3f minPoint = _points[0];
  GfVec3f maxPoint = _points[0];

  for(const auto& point : _points)
  {
    for(int i = 0; i < 3; ++i)
    {
      minPoint[i] = std::min(minPoint[i], point[i]);
      maxPoint[i] = std::max(maxPoint[i], point[i]);
    }
  }

  std::cout << std::fixed << std::setprecision(3);
  std::cout << "Bounding box:" << std::endl;
  std::cout << "  Min: (" << minPoint[0] << ", " << minPoint[1] << ", " << minPoint[2] << ")" << std::endl;
  std::cout << "  Max: (" << maxPoint[0] << ", " << maxPoint[1] << ", " << maxPoint[2] << ")" << std::endl;

  // 鎵撳嵃鍓?涓偣浣滀负绀轰緥
  size_t sampleCount = std::min(size_t(5), _points.size());
  std::cout << "First " << sampleCount << " points:" << std::endl;

  for(size_t i = 0; i < sampleCount; ++i)
  {
    const GfVec3f& point = _points[i];
    std::cout << "  [" << i << "]: (" << std::setw(8) << point[0] << ", " << std::setw(8) << point[1] << ", "
              << std::setw(8) << point[2] << ")";

    if(i < _widths.size())
    {
      std::cout << " w=" << _widths[i];
    }
    std::cout << std::endl;
  }

  if(_points.size() > sampleCount)
  {
    std::cout << "  ... and " << (_points.size() - sampleCount) << " more points" << std::endl;
  }

  std::cout << "======================" << std::endl;
}

PXR_NAMESPACE_CLOSE_SCOPE

