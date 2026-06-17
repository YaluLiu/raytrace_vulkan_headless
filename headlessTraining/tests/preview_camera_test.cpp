#include "preview_camera.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/norm.hpp>

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

bool Near(float lhs, float rhs, float epsilon = 1.0e-4f)
{
  return std::fabs(lhs - rhs) <= epsilon;
}

MeshGeometry MakeBoxGeometry(glm::vec3 minPoint, glm::vec3 maxPoint)
{
  MeshGeometry geometry;
  const glm::vec3 corners[] = {
      {minPoint.x, minPoint.y, minPoint.z}, {maxPoint.x, minPoint.y, minPoint.z},
      {minPoint.x, maxPoint.y, minPoint.z}, {maxPoint.x, maxPoint.y, minPoint.z},
      {minPoint.x, minPoint.y, maxPoint.z}, {maxPoint.x, minPoint.y, maxPoint.z},
      {minPoint.x, maxPoint.y, maxPoint.z}, {maxPoint.x, maxPoint.y, maxPoint.z},
  };
  for(glm::vec3 corner : corners)
  {
    MeshVertex vertex{};
    vertex.pos = corner;
    geometry.vertices.push_back(vertex);
  }
  return geometry;
}

headless_training::TrainingMeshInstance MakeMesh(std::string name, glm::vec3 translation, bool visible = true)
{
  headless_training::TrainingMeshInstance mesh;
  mesh.name = std::move(name);
  mesh.geometry = MakeBoxGeometry(glm::vec3(-0.5f, 0.0f, -0.5f), glm::vec3(0.5f, 1.0f, 0.5f));
  mesh.worldTransform = glm::translate(glm::mat4(1.0f), translation);
  mesh.visible = visible;
  return mesh;
}

float DistanceToTarget(const CameraSpec& camera, glm::vec3 target)
{
  return glm::length(camera.position - target);
}
} // namespace

int main()
{
  headless_training::TrainingSceneDescription scene;
  scene.meshes.push_back(MakeMesh("/World/DogA", glm::vec3(-2.0f, 0.0f, 0.0f)));
  scene.meshes.push_back(MakeMesh("/World/DogB", glm::vec3(2.0f, 0.0f, 0.0f)));
  scene.meshes.push_back(MakeMesh("/World/HiddenDog", glm::vec3(100.0f, 0.0f, 0.0f), false));

  const CameraSpec camera = headless_training::BuildPreviewCamera(scene);
  const glm::vec3 expectedTarget(0.0f, 0.5f, 0.0f);
  Require(glm::length(camera.forward) > 0.99f && glm::length(camera.forward) < 1.01f,
          "preview camera forward should be normalized");
  Require(glm::dot(camera.forward, glm::normalize(expectedTarget - camera.position)) > 0.999f,
          "preview camera should look at visible mesh bounds center");
  Require(camera.position.y > expectedTarget.y, "preview camera should be above the bounds center");
  Require(DistanceToTarget(camera, expectedTarget) > 3.0f, "preview camera should pull back from multi-dog bounds");

  headless_training::TrainingSceneDescription compactScene;
  compactScene.meshes.push_back(MakeMesh("/World/Dog", glm::vec3(0.0f)));
  const CameraSpec compactCamera = headless_training::BuildPreviewCamera(compactScene);
  Require(DistanceToTarget(camera, expectedTarget) > DistanceToTarget(compactCamera, glm::vec3(0.0f, 0.5f, 0.0f)),
          "larger bounds should produce a farther preview camera");

  headless_training::PreviewCameraOptions scaledOptions;
  scaledOptions.distanceScale = 2.0f;
  const CameraSpec scaledCamera = headless_training::BuildPreviewCamera(scene, scaledOptions);
  Require(DistanceToTarget(scaledCamera, expectedTarget) > DistanceToTarget(camera, expectedTarget),
          "distance scale should pull the automatic camera farther back");

  headless_training::PreviewCameraOptions overrideOptions;
  overrideOptions.position = glm::vec3(10.0f, 9.0f, 8.0f);
  overrideOptions.target = glm::vec3(1.0f, 2.0f, 3.0f);
  overrideOptions.verticalFovDegrees = 70.0f;
  const CameraSpec overrideCamera = headless_training::BuildPreviewCamera(scene, overrideOptions);
  Require(Near(overrideCamera.position.x, 10.0f) && Near(overrideCamera.position.y, 9.0f) &&
              Near(overrideCamera.position.z, 8.0f),
          "explicit preview camera position should win");
  Require(Near(overrideCamera.verticalFovDegrees, 70.0f), "explicit preview camera fov should win");
  Require(glm::dot(overrideCamera.forward, glm::normalize(*overrideOptions.target - *overrideOptions.position)) >
              0.999f,
          "explicit preview camera target should define forward direction");

  return 0;
}
