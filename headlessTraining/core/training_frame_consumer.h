#pragma once

#include "physics_fabric.h"
#include "training_scene.h"

#include <cstdint>

namespace headless_training
{

struct TrainingFrameApplyResult
{
  bool frameApplied{false};
  bool sensorOutputsDirty{false};
  uint64_t frameIndex{0};
};

TrainingFrameApplyResult ApplyLatestFabricFrame(PhysicsFabric& fabric,
                                                TrainingSceneRuntime& runtime,
                                                Engine& engine);

} // namespace headless_training
