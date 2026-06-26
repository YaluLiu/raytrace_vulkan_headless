#include "training_frame_consumer.h"

namespace headless_training
{

TrainingFrameApplyResult ApplyLatestFabricFrame(PhysicsFabric& fabric,
                                                TrainingSceneRuntime& runtime,
                                                Engine& engine)
{
  PhysicsFrameSnapshot snapshot;
  if(!fabric.consumeLatest(snapshot))
  {
    return {};
  }

  TrainingFrameApplyResult result;
  result.frameApplied = true;
  result.frameIndex = snapshot.frameIndex;
  result.sensorOutputsDirty = runtime.applyPoseUpdates(engine, snapshot.updates);
  return result;
}

} // namespace headless_training
