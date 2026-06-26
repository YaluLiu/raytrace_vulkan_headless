#pragma once

#include "training_scene.h"

#include <cstdint>

namespace headless_training
{

struct PhysicsFrameSnapshot
{
  uint64_t frameIndex{0};
  AnimationFrameUpdates updates;
};

enum class PhysicsFrameStatus
{
  Advanced,
  EndOfStream,
  NoSource,
};

class PhysicsFrameSource
{
public:
  virtual ~PhysicsFrameSource() = default;
  virtual PhysicsFrameStatus nextFrame(PhysicsFrameSnapshot& snapshot) = 0;
  virtual void reset() = 0;
};

} // namespace headless_training
