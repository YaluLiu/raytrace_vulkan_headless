#include "physics_fabric.h"

#include <utility>

namespace headless_training
{

void PhysicsFabric::clear()
{
  m_pendingFrame.reset();
}

void PhysicsFabric::publish(PhysicsFrameSnapshot snapshot)
{
  m_pendingFrame = std::move(snapshot);
}

bool PhysicsFabric::consumeLatest(PhysicsFrameSnapshot& snapshot)
{
  if(!m_pendingFrame.has_value())
  {
    return false;
  }
  snapshot = std::move(*m_pendingFrame);
  m_pendingFrame.reset();
  return true;
}

bool PhysicsFabric::hasFrame() const
{
  return m_pendingFrame.has_value();
}

} // namespace headless_training
