#pragma once

#include "training_scene.h"

#include <filesystem>
#include <memory>
#include <vector>

namespace headless_training
{

class PhysicsStateSource
{
public:
  virtual ~PhysicsStateSource() = default;
  virtual bool nextFrame(std::vector<InstancePoseUpdate>& updates) = 0;
};

std::unique_ptr<PhysicsStateSource> LoadPhysicsReplay(const std::filesystem::path& replayPath);

} // namespace headless_training
