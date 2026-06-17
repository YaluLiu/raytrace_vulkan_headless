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

headless_training::TrainingMeshInstance MakeMeshFromBounds(std::string name,
                                                           glm::vec3 minPoint,
                                                           glm::vec3 maxPoint,
                                                           uint32_t traceMask = kTraceMaskDefaultGeometry)
{
  headless_training::TrainingMeshInstance mesh;
  mesh.name = std::move(name);
  mesh.geometry = MakeBoxGeometry(minPoint, maxPoint);
  mesh.traceMask = traceMask;
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

  headless_training::TrainingSceneDescription zUpScene;
  zUpScene.meshes.push_back(MakeMeshFromBounds("/World/ground/terrain/mesh", glm::vec3(-10.0f, -13.0f, -0.05f),
                                               glm::vec3(10.0f, 13.0f, 0.05f), kTraceMaskGround));
  zUpScene.meshes.push_back(MakeMeshFromBounds("/World/envs/env_0/Robot/trunk/visuals",
                                               glm::vec3(0.8f, -12.3f, 0.0f), glm::vec3(1.3f, -11.5f, 0.5f)));
  zUpScene.meshes.push_back(MakeMeshFromBounds("/World/envs/env_1/Robot/trunk/visuals",
                                               glm::vec3(7.7f, 8.6f, -0.6f), glm::vec3(8.2f, 9.3f, 0.0f)));
  zUpScene.meshes.push_back(MakeMeshFromBounds("/World/envs/env_2/Robot/trunk/visuals",
                                               glm::vec3(-5.1f, -12.1f, 0.0f), glm::vec3(-4.7f, -11.3f, 0.5f)));
  HeightScanSensorSpec heightScan;
  heightScan.params.gravityDirectionWs = glm::vec3(0.0f, 0.0f, -1.0f);
  zUpScene.heightScanSensors.push_back(heightScan);

  const CameraSpec zUpCamera = headless_training::BuildPreviewCamera(zUpScene);
  const glm::vec3 zUpTarget(1.55f, -1.5f, -0.05f);
  Require(Near(zUpCamera.position.x, zUpTarget.x) && Near(zUpCamera.position.y, zUpTarget.y),
          "Z-up preview camera should center on non-ground meshes");
  Require(zUpCamera.position.z > zUpTarget.z, "Z-up preview camera should move opposite gravity");
  Require(glm::dot(zUpCamera.forward, glm::vec3(0.0f, 0.0f, -1.0f)) > 0.999f,
          "Z-up preview camera should look along gravity");
  Require(Near(zUpCamera.verticalFovDegrees, 90.0f), "Z-up preview camera should use the wide overview FOV");
  Require(std::fabs(glm::dot(zUpCamera.up, zUpCamera.forward)) < 1.0e-4f,
          "Z-up preview camera up vector should be perpendicular to forward");

  return 0;
}
