#pragma once

#include "training_scene.h"

#include <filesystem>
#include <memory>
#include <vector>

namespace headless_training
{

class AnimationStateSource
{
public:
  virtual ~AnimationStateSource() = default;
  virtual bool nextFrame(std::vector<InstancePoseUpdate>& updates) = 0;
  virtual void reset() = 0;
};

std::unique_ptr<AnimationStateSource> LoadUsdAnimationSource(const std::filesystem::path& usdPath,
                                                            const TrainingSceneDescription& scene);

} // namespace headless_training
