#include "physics_fabric.h"

#include <cstdlib>
#include <iostream>

#include <glm/gtc/matrix_transform.hpp>

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
  headless_training::PhysicsFrameSnapshot consumed;
  Require(!fabric.hasFrame(), "new fabric should not have a pending frame");
  Require(!fabric.consumeLatest(consumed), "empty fabric should not consume a frame");

  headless_training::PhysicsFrameSnapshot first;
  first.frameIndex = 7;
  headless_training::InstancePoseUpdate pose;
  pose.name = "/World/Robot";
  pose.worldTransform = glm::translate(glm::mat4(1.0f), glm::vec3(1.0f, 2.0f, 3.0f));
  first.updates.instancePoses.push_back(pose);
  fabric.publish(first);
  Require(fabric.hasFrame(), "published fabric should have a pending frame");
  Require(fabric.consumeLatest(consumed), "published fabric should consume one frame");
  Require(consumed.frameIndex == 7, "consumed frame index mismatch");
  Require(consumed.updates.instancePoses.size() == 1, "consumed instance update count mismatch");
  Require(consumed.updates.instancePoses[0].name == "/World/Robot", "consumed instance update name mismatch");
  Require(!fabric.hasFrame(), "consume should clear pending frame");
  Require(!fabric.consumeLatest(consumed), "fabric should be empty after consume");

  headless_training::PhysicsFrameSnapshot oldFrame;
  oldFrame.frameIndex = 8;
  headless_training::PhysicsFrameSnapshot latestFrame;
  latestFrame.frameIndex = 9;
  fabric.publish(oldFrame);
  fabric.publish(latestFrame);
  Require(fabric.consumeLatest(consumed), "latest frame should be consumable");
  Require(consumed.frameIndex == 9, "fabric should keep only the latest published frame");

  fabric.publish(latestFrame);
  fabric.clear();
  Require(!fabric.hasFrame(), "clear should drop pending frame");
  return 0;
}
