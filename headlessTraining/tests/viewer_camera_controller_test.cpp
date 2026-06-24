#include "viewer_camera_controller.h"

#include <engine/renderer_types.hpp>

#include <cmath>
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

bool Near(float lhs, float rhs, float epsilon = 1.0e-5f)
{
  return std::fabs(lhs - rhs) <= epsilon;
}

bool NearVec3(glm::vec3 lhs, glm::vec3 rhs, float epsilon = 1.0e-5f)
{
  return Near(lhs.x, rhs.x, epsilon) && Near(lhs.y, rhs.y, epsilon) && Near(lhs.z, rhs.z, epsilon);
}
} // namespace

int main()
{
  CameraSpec usdCamera;
  usdCamera.name = "/World/rolled_camera";
  usdCamera.position = glm::vec3(1.0f, 2.0f, 3.0f);
  usdCamera.forward = glm::vec3(0.0f, 0.0f, -1.0f);
  usdCamera.up = glm::vec3(1.0f, 0.0f, 0.0f);
  usdCamera.verticalFovDegrees = 40.0f;
  usdCamera.horizontalFovDegrees = 70.0f;
  usdCamera.clipStart = 0.25f;
  usdCamera.clipEnd = 250.0f;
  usdCamera.conformPolicy = CameraConformPolicy::Fit;

  headless_training::ViewerCameraController controller;
  controller.reset(usdCamera, usdCamera.position + usdCamera.forward * 5.0f);

  CameraSpec resolvedCamera = controller.camera();
  Require(NearVec3(resolvedCamera.position, usdCamera.position), "reset should preserve USD camera position");
  Require(NearVec3(resolvedCamera.forward, usdCamera.forward), "reset should preserve USD camera forward");
  Require(NearVec3(resolvedCamera.up, usdCamera.up), "reset should preserve USD camera roll/up");
  Require(Near(resolvedCamera.verticalFovDegrees, usdCamera.verticalFovDegrees),
          "reset should preserve USD camera vertical FOV");
  Require(Near(resolvedCamera.horizontalFovDegrees, usdCamera.horizontalFovDegrees),
          "reset should preserve USD camera horizontal FOV");
  Require(Near(resolvedCamera.clipStart, usdCamera.clipStart) && Near(resolvedCamera.clipEnd, usdCamera.clipEnd),
          "reset should preserve USD camera clip range");
  Require(resolvedCamera.conformPolicy == usdCamera.conformPolicy, "reset should preserve USD camera conform policy");

  controller.setVerticalFovDegrees(55.0f);
  resolvedCamera = controller.camera();
  Require(Near(resolvedCamera.verticalFovDegrees, 55.0f), "FOV control should update vertical FOV");
  Require(Near(resolvedCamera.horizontalFovDegrees, 0.0f), "FOV control should clear authored horizontal FOV");

  return 0;
}
