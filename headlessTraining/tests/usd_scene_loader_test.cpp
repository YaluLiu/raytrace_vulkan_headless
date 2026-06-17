#include "usd_scene_loader.h"

#include <engine/renderer_types.hpp>

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

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

std::filesystem::path WriteFixture()
{
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "headless_training_usd_scene_loader_test.usda";
  std::ofstream output(path, std::ios::out | std::ios::trunc);
  output << R"USD(#usda 1.0

def Xform "World"
{
    def Mesh "Ground"
    {
        custom token hdRobot:traceRole = "ground"
        point3f[] points = [(-1, 0, -1), (1, 0, -1), (1, 0, 1), (-1, 0, 1)]
        normal3f[] normals = [(1, 0, 0), (0, 1, 0), (0, 0, 1), (-1, 0, 0)]
        uniform token normalsInterpolation = "faceVarying"
        int[] faceVertexCounts = [4]
        int[] faceVertexIndices = [0, 1, 2, 3]
        matrix4d xformOp:transform = ((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (2, 3, 4, 1))
        uniform token[] xformOpOrder = ["xformOp:transform"]
    }

    def Camera "MainCamera"
    {
        float focalLength = 50
        float horizontalAperture = 20
        float verticalAperture = 10
        float2 clippingRange = (0.2, 500)
        matrix4d xformOp:transform = ((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (0, 1, 2, 1))
        uniform token[] xformOpOrder = ["xformOp:transform"]
    }

    def LidarSensor "FrontLidar"
    {
        custom float azimuthStepDeg = -2
        matrix4d xformOp:transform = ((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (1, 2, 3, 1))
        uniform token[] xformOpOrder = ["xformOp:transform"]
    }

    def LidarSensor "DisabledLidar"
    {
        custom bool enabled = false
    }

    def HeightScanSensor "Feet"
    {
        custom float uStep = -0.25
        custom float3 gravityDirectionWs = (0, 0, -2)
    }
}
)USD";
  return path;
}
} // namespace

int main()
{
  const std::filesystem::path fixture = WriteFixture();
  const headless_training::TrainingSceneDescription scene =
      headless_training::LoadUsdTrainingScene(fixture);
  std::filesystem::remove(fixture);

  Require(scene.meshes.size() == 1, "expected one mesh");
  const headless_training::TrainingMeshInstance& mesh = scene.meshes[0];
  Require(mesh.name == "/World/Ground", "mesh path mismatch");
  Require(mesh.traceMask == kTraceMaskGround, "ground traceRole did not map to ground trace mask");
  Require(mesh.geometry.vertices.size() == 6, "quad mesh should triangulate to six expanded vertices");
  Require(mesh.geometry.indices.size() == 6, "quad mesh should produce six indices");
  Require(mesh.geometry.materialIndices.size() == 2, "quad mesh should produce two material indices");
  Require(Near(mesh.geometry.vertices[0].nrm.x, 1.0f) && Near(mesh.geometry.vertices[1].nrm.y, 1.0f) &&
              Near(mesh.geometry.vertices[2].nrm.z, 1.0f) && Near(mesh.geometry.vertices[5].nrm.x, -1.0f),
          "face-varying normals should follow original face corners through triangulation");
  Require(Near(mesh.worldTransform[3][0], 2.0f) && Near(mesh.worldTransform[3][1], 3.0f) &&
              Near(mesh.worldTransform[3][2], 4.0f),
          "USD matrix did not convert to engine translation");

  Require(scene.cameras.size() == 1, "expected one camera");
  Require(scene.cameras[0].name == "/World/MainCamera", "camera path mismatch");
  Require(Near(scene.cameras[0].position.x, 0.0f) && Near(scene.cameras[0].position.y, 1.0f) &&
              Near(scene.cameras[0].position.z, 2.0f),
          "camera transform position mismatch");
  Require(Near(scene.cameras[0].forward.x, 0.0f) && Near(scene.cameras[0].forward.y, 0.0f) &&
              Near(scene.cameras[0].forward.z, -1.0f),
          "camera forward should use local -Z");

  Require(scene.lidarSensors.size() == 1, "disabled lidar should be skipped");
  const LidarSensorSpec& lidar = scene.lidarSensors[0];
  Require(lidar.name == "/World/FrontLidar", "lidar name mismatch");
  Require(Near(lidar.params.azimuthStepDeg, 2.0f), "lidar step should be sanitized to positive");
  Require(Near(lidar.params.verticalStartDeg, -85.0f), "lidar should use schema verticalStart default");
  Require(Near(lidar.params.verticalEndDeg, -35.0f), "lidar should use schema verticalEnd default");
  Require(Near(lidar.params.maxRange, 5.0f), "lidar should use schema maxRange default");
  Require(Near(lidar.position.x, 1.0f) && Near(lidar.position.y, 2.0f) && Near(lidar.position.z, 3.0f),
          "lidar transform position mismatch");

  Require(scene.heightScanSensors.size() == 1, "expected one height scan sensor");
  const HeightScanSensorSpec& heightScan = scene.heightScanSensors[0];
  Require(heightScan.name == "/World/Feet", "height scan name mismatch");
  Require(Near(heightScan.params.uStart, -0.2f), "height scan should use schema uStart default");
  Require(Near(heightScan.params.uEnd, 0.2f), "height scan should use schema uEnd default");
  Require(Near(heightScan.params.uStep, 0.25f), "height scan uStep should be sanitized to positive");
  Require(Near(heightScan.params.maxRange, 5.0f), "height scan should use schema maxRange default");
  Require(Near(heightScan.params.gravityDirectionWs.z, -1.0f), "height scan gravity should be normalized");

  headless_training::UsdSceneLoadOptions options;
  options.cameraPath = "/World/MainCamera";
  const std::filesystem::path selectedFixture = WriteFixture();
  const headless_training::TrainingSceneDescription selectedCameraScene =
      headless_training::LoadUsdTrainingScene(selectedFixture, options);
  std::filesystem::remove(selectedFixture);
  Require(selectedCameraScene.cameras.size() == 1 && selectedCameraScene.cameras[0].name == "/World/MainCamera",
          "camera path selection failed");

  return 0;
}
