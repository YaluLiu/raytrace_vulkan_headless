#include "usd_animation_source.h"

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usd/usdGeom/xformable.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

namespace headless_training
{
namespace
{
constexpr float kSensorPoseEpsilon = 1.0e-6f;

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

glm::vec3 ToGlm(const PXR_NS::GfVec3d& value)
{
  return glm::vec3(static_cast<float>(value[0]), static_cast<float>(value[1]),
                   static_cast<float>(value[2]));
}

glm::vec3 NormalizeOr(glm::vec3 value, glm::vec3 fallback)
{
  if(!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z) ||
     glm::dot(value, value) < kSensorPoseEpsilon * kSensorPoseEpsilon)
  {
    value = fallback;
  }
  if(glm::dot(value, value) < kSensorPoseEpsilon * kSensorPoseEpsilon)
  {
    return glm::vec3(0.0f, 0.0f, -1.0f);
  }
  return glm::normalize(value);
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

void AddPrimWithTimeSamples(const std::string& name,
                            const PXR_NS::UsdStageRefPtr& stage,
                            std::vector<AnimatedPrim>& prims,
                            std::set<double>& timeSamples)
{
  PXR_NS::UsdPrim prim = stage->GetPrimAtPath(PXR_NS::SdfPath(name));
  if(!prim)
  {
    return;
  }

  prims.push_back(AnimatedPrim{name, prim});
  for(PXR_NS::UsdPrim samplePrim = prim; samplePrim && !samplePrim.IsPseudoRoot();
      samplePrim = samplePrim.GetParent())
  {
    AddTimeSamples(samplePrim, timeSamples);
  }
}

SensorPoseUpdate MakeSensorPoseUpdate(const AnimatedPrim& animatedPrim, PXR_NS::UsdTimeCode time)
{
  const PXR_NS::GfMatrix4d transform =
      PXR_NS::UsdGeomXformable(animatedPrim.prim).ComputeLocalToWorldTransform(time);

  SensorPoseUpdate update;
  update.name = animatedPrim.name;
  update.position = ToGlm(transform.Transform(PXR_NS::GfVec3d(0.0, 0.0, 0.0)));
  update.forward = NormalizeOr(ToGlm(transform.TransformDir(PXR_NS::GfVec3d(0.0, 0.0, -1.0))),
                               glm::vec3(0.0f, 0.0f, -1.0f));
  update.up = NormalizeOr(ToGlm(transform.TransformDir(PXR_NS::GfVec3d(0.0, 1.0, 0.0))),
                          glm::vec3(0.0f, 1.0f, 0.0f));
  if(glm::dot(glm::cross(update.forward, update.up), glm::cross(update.forward, update.up)) <
     kSensorPoseEpsilon * kSensorPoseEpsilon)
  {
    update.up = glm::vec3(0.0f, 1.0f, 0.0f);
  }
  return update;
}

class UsdAnimationStateSource final : public AnimationStateSource
{
public:
  UsdAnimationStateSource(PXR_NS::UsdStageRefPtr stage,
                          std::vector<AnimatedPrim> meshPrims,
                          std::vector<AnimatedPrim> lidarSensorPrims,
                          std::vector<AnimatedPrim> heightScanSensorPrims,
                          std::vector<double> timeSamples)
      : m_stage(std::move(stage))
      , m_meshPrims(std::move(meshPrims))
      , m_lidarSensorPrims(std::move(lidarSensorPrims))
      , m_heightScanSensorPrims(std::move(heightScanSensorPrims))
      , m_timeSamples(std::move(timeSamples))
  {
  }

  bool nextFrame(AnimationFrameUpdates& updates) override
  {
    if(m_nextFrame >= m_timeSamples.size())
    {
      updates = {};
      return false;
    }

    const PXR_NS::UsdTimeCode time(m_timeSamples[m_nextFrame]);
    updates.instancePoses.clear();
    updates.lidarSensorPoses.clear();
    updates.heightScanSensorPoses.clear();
    updates.instancePoses.reserve(m_meshPrims.size());
    updates.lidarSensorPoses.reserve(m_lidarSensorPrims.size());
    updates.heightScanSensorPoses.reserve(m_heightScanSensorPrims.size());
    for(const AnimatedPrim& animatedPrim : m_meshPrims)
    {
      InstancePoseUpdate update;
      update.name = animatedPrim.name;
      update.worldTransform = MakeEngineTransformFromUsdRows(ToRowMajorArray(
          PXR_NS::UsdGeomXformable(animatedPrim.prim).ComputeLocalToWorldTransform(time)));
      update.visible = IsVisibleAtTime(animatedPrim.prim, time);
      updates.instancePoses.push_back(update);
    }
    for(const AnimatedPrim& animatedPrim : m_lidarSensorPrims)
    {
      updates.lidarSensorPoses.push_back(MakeSensorPoseUpdate(animatedPrim, time));
    }
    for(const AnimatedPrim& animatedPrim : m_heightScanSensorPrims)
    {
      updates.heightScanSensorPoses.push_back(MakeSensorPoseUpdate(animatedPrim, time));
    }

    ++m_nextFrame;
    return true;
  }

  void reset() override { m_nextFrame = 0; }

private:
  PXR_NS::UsdStageRefPtr m_stage;
  std::vector<AnimatedPrim> m_meshPrims;
  std::vector<AnimatedPrim> m_lidarSensorPrims;
  std::vector<AnimatedPrim> m_heightScanSensorPrims;
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
  std::vector<AnimatedPrim> meshPrims;
  std::vector<AnimatedPrim> lidarSensorPrims;
  std::vector<AnimatedPrim> heightScanSensorPrims;
  meshPrims.reserve(scene.meshes.size());
  lidarSensorPrims.reserve(scene.lidarSensors.size());
  heightScanSensorPrims.reserve(scene.heightScanSensors.size());

  for(const TrainingMeshInstance& mesh : scene.meshes)
  {
    AddPrimWithTimeSamples(mesh.name, stage, meshPrims, timeSamples);
  }

  for(const LidarSensorSpec& sensor : scene.lidarSensors)
  {
    AddPrimWithTimeSamples(sensor.name, stage, lidarSensorPrims, timeSamples);
  }

  for(const HeightScanSensorSpec& sensor : scene.heightScanSensors)
  {
    AddPrimWithTimeSamples(sensor.name, stage, heightScanSensorPrims, timeSamples);
  }

  if((meshPrims.empty() && lidarSensorPrims.empty() && heightScanSensorPrims.empty()) || timeSamples.empty())
  {
    return nullptr;
  }

  return std::make_unique<UsdAnimationStateSource>(std::move(stage),
                                                  std::move(meshPrims),
                                                  std::move(lidarSensorPrims),
                                                  std::move(heightScanSensorPrims),
                                                  std::vector<double>(timeSamples.begin(), timeSamples.end()));
}

} // namespace headless_training
