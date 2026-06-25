#pragma once

#include "training_scene.h"

#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

namespace headless_training
{

class AnimationStateSource
{
public:
  virtual ~AnimationStateSource() = default;
  virtual bool nextFrame(AnimationFrameUpdates& updates) = 0;
  bool nextFrame(std::vector<InstancePoseUpdate>& updates)
  {
    AnimationFrameUpdates frameUpdates;
    const bool result = nextFrame(frameUpdates);
    updates = std::move(frameUpdates.instancePoses);
    return result;
  }
  virtual void reset() = 0;
};

std::unique_ptr<AnimationStateSource> LoadUsdAnimationSource(const std::filesystem::path& usdPath,
                                                            const TrainingSceneDescription& scene);

} // namespace headless_training
