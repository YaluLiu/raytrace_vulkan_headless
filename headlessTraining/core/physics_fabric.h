#pragma once

#include "physics_frame.h"

#include <optional>

namespace headless_training
{

class PhysicsFabric
{
public:
  void clear();
  void publish(PhysicsFrameSnapshot snapshot);
  bool consumeLatest(PhysicsFrameSnapshot& snapshot);
  bool hasFrame() const;

private:
  std::optional<PhysicsFrameSnapshot> m_pendingFrame;
};

} // namespace headless_training
