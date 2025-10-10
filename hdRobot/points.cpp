#include "points.h"
#include "pxr/imaging/hd/sceneDelegate.h"
#include "pxr/imaging/hd/repr.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/base/gf/matrix4d.h"
#include <iostream>
#include <iomanip>

PXR_NAMESPACE_OPEN_SCOPE

HdGatlingPoints::HdGatlingPoints(const SdfPath& id, HdGatlingScene& scene)
    : HdPoints(id)
    , _scene(scene)
    , _visible(true)
{
}

HdGatlingPoints::~HdGatlingPoints() {}

HdDirtyBits HdGatlingPoints::GetInitialDirtyBitsMask() const
{
  return HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyWidths | HdChangeTracker::DirtyPrimvar
         | HdChangeTracker::DirtyTransform | HdChangeTracker::DirtyVisibility | HdChangeTracker::DirtyRepr;
}

void HdGatlingPoints::_InitRepr(TfToken const& reprToken, HdDirtyBits* dirtyBits)
{
  // 简单的实现 - 让基类处理repr初始化
  // 在大多数情况下，这就足够了
  TF_UNUSED(reprToken);
  *dirtyBits |= HdChangeTracker::DirtyRepr;
}

HdDirtyBits HdGatlingPoints::_PropagateDirtyBits(HdDirtyBits bits) const
{
  return bits;
}

void HdGatlingPoints::Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits, TfToken const& reprToken)
{
  TF_UNUSED(renderParam);
  TF_UNUSED(reprToken);

  SdfPath const& id = GetId();

  // 更新几何数据
  if(*dirtyBits & (HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyWidths | HdChangeTracker::DirtyPrimvar))
  {
    _UpdateGeometry(sceneDelegate, dirtyBits);
  }

  // 更新变换
  if(*dirtyBits & HdChangeTracker::DirtyTransform)
  {
    _UpdateTransform(sceneDelegate, dirtyBits);
  }

  // 更新可见性
  if(*dirtyBits & HdChangeTracker::DirtyVisibility)
  {
    _UpdateVisibility(sceneDelegate, dirtyBits);
  }

  // 清除脏位
  *dirtyBits = HdChangeTracker::Clean;
}

void HdGatlingPoints::_UpdateGeometry(HdSceneDelegate* sceneDelegate, HdDirtyBits* dirtyBits)
{
  SdfPath const& id = GetId();

  // 读取点位置（必需）
  if(*dirtyBits & HdChangeTracker::DirtyPoints)
  {
    VtValue pointsValue = sceneDelegate->Get(id, HdTokens->points);
    if(pointsValue.IsHolding<VtVec3fArray>())
    {
      _points = pointsValue.UncheckedGet<VtVec3fArray>();
      std::cout << "Points loaded: " << _points.size() << " points" << std::endl;
    }
  }

  // 读取点宽度（必需）
  if(*dirtyBits & HdChangeTracker::DirtyWidths)
  {
    VtValue widthsValue = sceneDelegate->Get(id, HdTokens->widths);
    if(widthsValue.IsHolding<VtFloatArray>())
    {
      _widths = widthsValue.UncheckedGet<VtFloatArray>();
      std::cout << "Widths loaded: " << _widths.size() << " widths" << std::endl;
    }
    else
    {
      // 如果没有宽度数据，使用默认值
      _widths.assign(_points.size(), 1.0f);
      std::cout << "Using default widths for " << _points.size() << " points" << std::endl;
    }
  }

  // 读取颜色（可选）
  if(*dirtyBits & HdChangeTracker::DirtyPrimvar)
  {
    // 尝试获取displayColor
    VtValue colorValue = sceneDelegate->Get(id, HdTokens->displayColor);
    if(colorValue.IsHolding<VtVec3fArray>())
    {
      _colors = colorValue.UncheckedGet<VtVec3fArray>();
      std::cout << "Colors loaded: " << _colors.size() << " colors" << std::endl;
    }
    else
    {
      // 使用默认白色
      _colors.assign(_points.size(), GfVec3f(1.0f, 1.0f, 1.0f));
      std::cout << "Using default white color for " << _points.size() << " points" << std::endl;
    }
  }

  // 打印点数据
  if(*dirtyBits & HdChangeTracker::DirtyPoints)
  {
    _UpdateRenderScene();
    // _PrintPointsDetailed();
  }
}

void HdGatlingPoints::_UpdateTransform(HdSceneDelegate* sceneDelegate, HdDirtyBits* dirtyBits)
{
  SdfPath const& id = GetId();
  _transform        = sceneDelegate->GetTransform(id);

  std::cout << "Transform updated for " << id << std::endl;
}

void HdGatlingPoints::_UpdateVisibility(HdSceneDelegate* sceneDelegate, HdDirtyBits* dirtyBits)
{
  SdfPath const& id = GetId();
  _visible          = sceneDelegate->GetVisible(id);

  std::cout << "Visibility updated: " << (_visible ? "visible" : "hidden") << std::endl;
}

void HdGatlingPoints::_UpdateRenderScene() const
{
  int points_size = _points.size();
  _scene.v_sphere.resize(points_size);
  for(size_t i = 0; i < points_size; ++i)
  {
    const GfVec3f& point      = _points[i];
    _scene.v_sphere[i].center = glm::vec3(point[0], point[1], point[2]);
    _scene.v_sphere[i].radius = _widths[i];
  }
}

// 打印函数实现
void HdGatlingPoints::_PrintPoints() const
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

void HdGatlingPoints::_PrintPointsDetailed() const
{
  std::cout << "=== Detailed Points Data ===" << std::endl;
  std::cout << "Total points: " << _points.size() << std::endl;
  std::cout << "Widths count: " << _widths.size() << std::endl;
  std::cout << "Colors count: " << _colors.size() << std::endl;
  std::cout << std::fixed << std::setprecision(3);

  for(size_t i = 0; i < 20; ++i)
  {
    const GfVec3f& point = _points[i];

    std::cout << "Point[" << std::setw(4) << i << "]: "
              << "pos(" << std::setw(8) << point[0] << ", " << std::setw(8) << point[1] << ", " << std::setw(8)
              << point[2] << ")";

    // 打印宽度信息
    if(i < _widths.size())
    {
      std::cout << " width(" << std::setw(6) << _widths[i] << ")";
    }

    // 打印颜色信息
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

void HdGatlingPoints::_PrintPointsSummary() const
{
  std::cout << "=== Points Summary ===" << std::endl;
  std::cout << "Total points: " << _points.size() << std::endl;

  if(_points.empty())
  {
    std::cout << "No points data available." << std::endl;
    std::cout << "======================" << std::endl;
    return;
  }

  // 计算边界框
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

  // 打印前5个点作为示例
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
