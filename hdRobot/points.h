#ifndef HDMYRENDERER_POINTS_H
#define HDMYRENDERER_POINTS_H

#include "pxr/imaging/hd/points.h"
#include "pxr/imaging/hd/enums.h"
#include "pxr/imaging/hd/vertexAdjacency.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/vt/array.h"
#include "renderScene.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdGatlingPoints final : public HdPoints
{
public:
  HdGatlingPoints(const SdfPath& id, HdGatlingScene& _scene);
  virtual ~HdGatlingPoints();

  // 同步数据从场景图到渲染器
  virtual void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits, TfToken const& reprToken) override;

  // 获取初始脏位
  virtual HdDirtyBits GetInitialDirtyBitsMask() const override;

  void _UpdateRenderScene() const;

protected:
  // 更新表示
  virtual void _InitRepr(TfToken const& reprToken, HdDirtyBits* dirtyBits) override;

  // 脏位传播
  virtual HdDirtyBits _PropagateDirtyBits(HdDirtyBits bits) const override;

private:
  // 渲染数据
  VtVec3fArray _points;  // 点位置
  VtFloatArray _widths;  // 点宽度
  VtVec3fArray _colors;  // 点颜色

  // 变换矩阵
  GfMatrix4d _transform;

  // 可见性
  bool _visible;

  // 更新几何数据
  void _UpdateGeometry(HdSceneDelegate* sceneDelegate, HdDirtyBits* dirtyBits);

  // 更新变换
  void _UpdateTransform(HdSceneDelegate* sceneDelegate, HdDirtyBits* dirtyBits);

  // 更新可见性
  void _UpdateVisibility(HdSceneDelegate* sceneDelegate, HdDirtyBits* dirtyBits);

  // 调试函数
  void _PrintPoints() const;
  void _PrintPointsDetailed() const;
  void _PrintPointsSummary() const;

  HdGatlingScene& _scene;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDMYRENDERER_POINTS_H
