#include "training_frame_consumer.h"

#include <cstdlib>
#include <iostream>

namespace
{
void Require(bool condition, const char* message)
{
  if(condition)
  {
    return;
  }
  std::cerr << message << '\n';
  std::exit(1);
}
} // namespace

int main()
{
  headless_training::PhysicsFabric fabric;
  headless_training::TrainingSceneRuntime runtime(headless_training::TrainingSceneDescription{});
  Engine engine;

  const headless_training::TrainingFrameApplyResult result =
      headless_training::ApplyLatestFabricFrame(fabric, runtime, engine);
  Require(!result.frameApplied, "empty fabric should not apply a training frame");
  Require(!result.sensorOutputsDirty, "empty fabric should not dirty sensor outputs");
  Require(result.frameIndex == 0, "empty fabric result should use default frame index");
  return 0;
}
