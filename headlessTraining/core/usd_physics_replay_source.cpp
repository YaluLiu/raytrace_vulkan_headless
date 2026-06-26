#include "usd_physics_replay_source.h"

#include "usd_scene_reader_utils.h"

#include <pxr/base/gf/vec3d.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usd/timeCode.h>
#include <pxr/usd/usdGeom/imageable.h>
#include <pxr/usd/usdGeom/xformable.h>

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
struct AnimatedPrim
{
  std::string name;
  PXR_NS::UsdPrim prim;
};

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
     kSensorEpsilon * kSensorEpsilon)
  {
    update.up = glm::vec3(0.0f, 1.0f, 0.0f);
  }
  return update;
}

class UsdPhysicsReplaySource final : public PhysicsFrameSource
{
public:
  UsdPhysicsReplaySource(PXR_NS::UsdStageRefPtr stage,
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

  PhysicsFrameStatus nextFrame(PhysicsFrameSnapshot& snapshot) override
  {
    if(m_timeSamples.empty())
    {
      snapshot = {};
      return PhysicsFrameStatus::NoSource;
    }
    if(m_nextFrame >= m_timeSamples.size())
    {
      snapshot = {};
      return PhysicsFrameStatus::EndOfStream;
    }

    const size_t frameIndex = m_nextFrame;
    const PXR_NS::UsdTimeCode time(m_timeSamples[frameIndex]);
    AnimationFrameUpdates updates;
    updates.instancePoses.reserve(m_meshPrims.size());
    updates.lidarSensorPoses.reserve(m_lidarSensorPrims.size());
    updates.heightScanSensorPoses.reserve(m_heightScanSensorPrims.size());
    for(const AnimatedPrim& animatedPrim : m_meshPrims)
    {
      InstancePoseUpdate update;
      update.name = animatedPrim.name;
      update.worldTransform = ToEngineTransform(
          PXR_NS::UsdGeomXformable(animatedPrim.prim).ComputeLocalToWorldTransform(time));
      update.visible = !IsInvisible(animatedPrim.prim, time);
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

    snapshot.frameIndex = static_cast<uint64_t>(frameIndex);
    snapshot.updates = std::move(updates);
    ++m_nextFrame;
    return PhysicsFrameStatus::Advanced;
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

std::unique_ptr<PhysicsFrameSource> LoadUsdPhysicsReplaySource(const std::filesystem::path& usdPath,
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

  return std::make_unique<UsdPhysicsReplaySource>(std::move(stage),
                                                  std::move(meshPrims),
                                                  std::move(lidarSensorPrims),
                                                  std::move(heightScanSensorPrims),
                                                  std::vector<double>(timeSamples.begin(), timeSamples.end()));
}

} // namespace headless_training
