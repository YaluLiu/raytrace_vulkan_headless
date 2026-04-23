#ifndef HDMYRENDERER_POINTS_H
#define HDMYRENDERER_POINTS_H

#include "pxr/imaging/hd/points.h"
#include "pxr/imaging/hd/enums.h"
#include "pxr/imaging/hd/vertexAdjacency.h"
#include "pxr/base/gf/vec3f.h"
#include "pxr/base/vt/array.h"
#include "renderParam.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotPoints final : public HdPoints
{
public:
  HdRobotPoints(const SdfPath& id, HdRobotRenderParam& _scene);
  virtual ~HdRobotPoints();

  // 鍚屾鏁版嵁浠庡満鏅浘鍒版覆鏌撳櫒
  virtual void Sync(HdSceneDelegate* sceneDelegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits, TfToken const& reprToken) override;

  // 鑾峰彇鍒濆鑴忎綅
  virtual HdDirtyBits GetInitialDirtyBitsMask() const override;

  void _UpdateRenderScene() const;

protected:
  // 鏇存柊琛ㄧず
  virtual void _InitRepr(TfToken const& reprToken, HdDirtyBits* dirtyBits) override;

  // 鑴忎綅浼犳挱
  virtual HdDirtyBits _PropagateDirtyBits(HdDirtyBits bits) const override;

private:
  // 娓叉煋鏁版嵁
  VtVec3fArray _points;  // 鐐逛綅缃?
  VtFloatArray _widths;  // 鐐瑰搴?
  VtVec3fArray _colors;  // 鐐归鑹?

  // 鍙樻崲鐭╅樀
  GfMatrix4d _transform;

  // 鍙鎬?
  bool _visible;

  // 鏇存柊鍑犱綍鏁版嵁
  void _UpdateGeometry(HdSceneDelegate* sceneDelegate, HdDirtyBits* dirtyBits);

  // 鏇存柊鍙樻崲
  void _UpdateTransform(HdSceneDelegate* sceneDelegate, HdDirtyBits* dirtyBits);

  // 鏇存柊鍙鎬?
  void _UpdateVisibility(HdSceneDelegate* sceneDelegate, HdDirtyBits* dirtyBits);

  // 璋冭瘯鍑芥暟
  void _PrintPoints() const;
  void _PrintPointsDetailed() const;
  void _PrintPointsSummary() const;

  HdRobotRenderParam& _scene;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif  // HDMYRENDERER_POINTS_H

