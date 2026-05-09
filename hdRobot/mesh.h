#pragma once

#include <pxr/imaging/hd/mesh.h>
#include <pxr/base/gf/vec2f.h>
#include <optional>
#include "renderParam.h"

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotMesh final : public HdMesh
{
public:
  HdRobotMesh(const SdfPath& id, HdRobotRenderParam& _scene);

  void Finalize(HdRenderParam* renderParam) override;
  void setValid(bool value);

public:
  void Sync(HdSceneDelegate* delegate, HdRenderParam* renderParam, HdDirtyBits* dirtyBits, const TfToken& reprToken) override;

  HdDirtyBits GetInitialDirtyBitsMask() const override;

protected:
  HdDirtyBits _PropagateDirtyBits(HdDirtyBits bits) const override;

  void _InitRepr(const TfToken& reprName, HdDirtyBits* dirtyBits) override;

private:
  void _AnalyzePrimvars(HdSceneDelegate* sceneDelegate, bool& foundNormals, bool& indexingAllowed);
  void GetDisplayColor(HdSceneDelegate* sceneDelegate);

  struct ProcessedPrimvar
  {
    HdInterpolation interpolation;
    HdType          type;
    TfToken         role;
    VtValue         indexMatchingData;
  };

  using PrimvarMap = std::unordered_map<TfToken, ProcessedPrimvar, TfToken::HashFunctor>;

  std::optional<ProcessedPrimvar> _ProcessPrimvar(HdSceneDelegate*           sceneDelegate,
                                                  const VtIntArray&          primitiveParams,
                                                  const HdPrimvarDescriptor& primvarDesc,
                                                  const VtVec3iArray&        faces,
                                                  uint32_t                   vertexCount,
                                                  bool                       indexingAllowed,
                                                  bool                       constantAllowed);

  PrimvarMap _ProcessPrimvars(HdSceneDelegate*    sceneDelegate,
                              const VtIntArray&   primitiveParams,
                              const VtVec3iArray& faces,
                              uint32_t            vertexCount,
                              bool                indexingAllowed);

  void _CreateGiMeshes(HdSceneDelegate* sceneDelegate);

private:
  HdRobotRenderParam& _scene;
  int             _mesh_id = -1;
  int             _display_color_mat_id = -1;
};

PXR_NAMESPACE_CLOSE_SCOPE
