#include "usd_animation_source.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformable.h>

#include <algorithm>
#include <array>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace headless_training
{
namespace
{
std::array<double, 16> ToRowMajorArray(const PXR_NS::GfMatrix4d& matrix)
{
  std::array<double, 16> values{};
  for(int row = 0; row < 4; ++row)
  {
    for(int column = 0; column < 4; ++column)
    {
      values[static_cast<size_t>(row * 4 + column)] = matrix[row][column];
    }
  }
  return values;
}

bool IsVisibleAtTime(const PXR_NS::UsdPrim& prim, PXR_NS::UsdTimeCode time)
{
  PXR_NS::UsdGeomImageable imageable(prim);
  return !imageable || imageable.ComputeVisibility(time) != PXR_NS::UsdGeomTokens->invisible;
}

void AddTimeSamples(const PXR_NS::UsdPrim& prim, std::set<double>& timeSamples)
{
  if(!prim)
  {
    return;
  }

  PXR_NS::UsdGeomXformable xformable(prim);
  if(xformable)
  {
    std::vector<double> primTimes;
    if(xformable.GetTimeSamples(&primTimes))
    {
      timeSamples.insert(primTimes.begin(), primTimes.end());
    }
  }

  PXR_NS::UsdGeomImageable imageable(prim);
  if(imageable)
  {
    std::vector<double> visibilityTimes;
    if(imageable.GetVisibilityAttr().GetTimeSamples(&visibilityTimes))
    {
      timeSamples.insert(visibilityTimes.begin(), visibilityTimes.end());
    }
  }
}

struct AnimatedPrim
{
  std::string name;
  PXR_NS::UsdPrim prim;
};

class UsdAnimationStateSource final : public AnimationStateSource
{
public:
  UsdAnimationStateSource(PXR_NS::UsdStageRefPtr stage, std::vector<AnimatedPrim> prims,
                          std::vector<double> timeSamples)
      : m_stage(std::move(stage))
      , m_prims(std::move(prims))
      , m_timeSamples(std::move(timeSamples))
  {
  }

  bool nextFrame(std::vector<InstancePoseUpdate>& updates) override
  {
    if(m_nextFrame >= m_timeSamples.size())
    {
      updates.clear();
      return false;
    }

    const PXR_NS::UsdTimeCode time(m_timeSamples[m_nextFrame]);
    updates.clear();
    updates.reserve(m_prims.size());
    for(const AnimatedPrim& animatedPrim : m_prims)
    {
      InstancePoseUpdate update;
      update.name = animatedPrim.name;
      update.worldTransform = MakeEngineTransformFromUsdRows(ToRowMajorArray(
          PXR_NS::UsdGeomXformable(animatedPrim.prim).ComputeLocalToWorldTransform(time)));
      update.visible = IsVisibleAtTime(animatedPrim.prim, time);
      updates.push_back(update);
    }

    ++m_nextFrame;
    return true;
  }

  void reset() override { m_nextFrame = 0; }

private:
  PXR_NS::UsdStageRefPtr m_stage;
  std::vector<AnimatedPrim> m_prims;
  std::vector<double> m_timeSamples;
  size_t m_nextFrame{0};
};
} // namespace

std::unique_ptr<AnimationStateSource> LoadUsdAnimationSource(const std::filesystem::path& usdPath,
                                                            const TrainingSceneDescription& scene)
{
  PXR_NS::UsdStageRefPtr stage = PXR_NS::UsdStage::Open(usdPath.string());
  if(!stage)
  {
    throw std::runtime_error("failed to open USD stage: " + usdPath.string());
  }

  std::set<double> timeSamples;
  std::vector<AnimatedPrim> prims;
  prims.reserve(scene.meshes.size());

  for(const TrainingMeshInstance& mesh : scene.meshes)
  {
    PXR_NS::UsdPrim prim = stage->GetPrimAtPath(PXR_NS::SdfPath(mesh.name));
    if(!prim)
    {
      continue;
    }

    prims.push_back(AnimatedPrim{mesh.name, prim});
    for(PXR_NS::UsdPrim samplePrim = prim; samplePrim && !samplePrim.IsPseudoRoot();
        samplePrim = samplePrim.GetParent())
    {
      AddTimeSamples(samplePrim, timeSamples);
    }
  }

  if(prims.empty() || timeSamples.empty())
  {
    return nullptr;
  }

  return std::make_unique<UsdAnimationStateSource>(std::move(stage), std::move(prims),
                                                  std::vector<double>(timeSamples.begin(), timeSamples.end()));
}

} // namespace headless_training
