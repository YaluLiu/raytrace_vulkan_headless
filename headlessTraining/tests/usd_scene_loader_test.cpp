#include "usd_scene_loader.h"

#include <engine/renderer_types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
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
  const std::filesystem::path texturePath =
      std::filesystem::temp_directory_path() / "headless_training_usd_scene_loader_test_texture.png";
  {
    std::ofstream texture(texturePath, std::ios::out | std::ios::binary | std::ios::trunc);
    const unsigned char pngSignature[8] = {0x89u, 0x50u, 0x4eu, 0x47u, 0x0du, 0x0au, 0x1au, 0x0au};
    texture.write(reinterpret_cast<const char*>(pngSignature), sizeof(pngSignature));
  }

  std::ofstream output(path, std::ios::out | std::ios::trunc);
  output << R"USD(#usda 1.0

def Xform "World"
{
    def Scope "Looks"
    {
        def Material "PreviewMaterial"
        {
            token outputs:surface.connect = </World/Looks/PreviewMaterial/Shader.outputs:surface>

            def Shader "Shader"
            {
                uniform token info:id = "UsdPreviewSurface"
                color3f inputs:diffuseColor = (0.18, 0.18, 0.18)
                color3f inputs:diffuseColor.connect = </World/Looks/PreviewMaterial/albedoTexture.outputs:rgb>
                color3f inputs:emissiveColor = (0.1, 0.2, 0.3)
                float inputs:metallic = 0.25
                float inputs:opacity = 0.75
                float inputs:roughness = 0.33
                token outputs:surface
            }

            def Shader "albedoTexture"
            {
                uniform token info:id = "UsdUVTexture"
                asset inputs:file = @)USD"
         << texturePath.generic_string() << R"USD(@
                float3 outputs:rgb
            }
        }
    }

    def Mesh "Ground" (
        prepend apiSchemas = ["MaterialBindingAPI"]
    )
    {
        custom token hdRobot:traceRole = "ground"
        rel material:binding = </World/Looks/PreviewMaterial>
        point3f[] points = [(-1, 0, -1), (1, 0, -1), (1, 0, 1), (-1, 0, 1)]
        normal3f[] normals = [(1, 0, 0), (0, 1, 0), (0, 0, 1), (-1, 0, 0)]
        uniform token normalsInterpolation = "faceVarying"
        texCoord2f[] primvars:st = [(0, 0), (1, 0), (1, 1), (0, 1)] (
            interpolation = "vertex"
        )
        int[] faceVertexCounts = [4]
        int[] faceVertexIndices = [0, 1, 2, 3]
        matrix4d xformOp:transform = ((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (2, 3, 4, 1))
        uniform token[] xformOpOrder = ["xformOp:transform"]
    }

    def Mesh "DisplayColorMesh"
    {
        color3f[] primvars:displayColor = [(0.4, 0.5, 0.6)]
        float[] primvars:displayOpacity = [0.7]
        point3f[] points = [(0, 0, 0), (1, 0, 0), (0, 1, 0)]
        int[] faceVertexCounts = [3]
        int[] faceVertexIndices = [0, 1, 2]
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

    def Camera "RearCamera"
    {
        float focalLength = 35
        float horizontalAperture = 24
        float verticalAperture = 18
        float2 clippingRange = (0.1, 250)
        matrix4d xformOp:transform = ((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (3, 4, 5, 1))
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

    def DomeLight "Sky"
    {
        color3f inputs:color = (0.2, 0.3, 0.4)
        float inputs:intensity = 2
        asset inputs:texture:file = @)USD"
         << texturePath.generic_string() << R"USD(@
    }

    def SphereLight "Bulb"
    {
        color3f inputs:color = (1, 0.5, 0.25)
        float inputs:intensity = 3
        float inputs:radius = 2
        matrix4d xformOp:transform = ((1, 0, 0, 0), (0, 1, 0, 0), (0, 0, 1, 0), (5, 6, 7, 1))
        uniform token[] xformOpOrder = ["xformOp:transform"]
    }
}
)USD";
  return path;
}

std::filesystem::path WriteStartTimeFixture()
{
  const std::filesystem::path path =
      std::filesystem::temp_directory_path() / "headless_training_usd_scene_loader_start_time_test.usda";

  std::ofstream output(path, std::ios::out | std::ios::trunc);
  output << R"USD(#usda 1.0
(
    startTimeCode = 0
    endTimeCode = 10
)

def Xform "World"
{
    def Mesh "AnimatedMesh"
    {
        point3f[] points = [(0, 0, 0), (1, 0, 0), (0, 1, 0)]
        int[] faceVertexCounts = [3]
        int[] faceVertexIndices = [0, 1, 2]
        double3 xformOp:translate = (100, 0, 0)
        double3 xformOp:translate.timeSamples = {
            0: (1, 2, 3),
            10: (4, 5, 6),
        }
        uniform token[] xformOpOrder = ["xformOp:translate"]
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
  std::filesystem::remove(std::filesystem::temp_directory_path() /
                          "headless_training_usd_scene_loader_test_texture.png");

  Require(scene.meshes.size() == 2, "expected two meshes");
  const headless_training::TrainingMeshInstance& mesh = scene.meshes[0];
  Require(mesh.name == "/World/Ground", "mesh path mismatch");
  Require(mesh.traceMask == kTraceMaskGround, "ground traceRole did not map to ground trace mask");
  Require(mesh.geometry.vertices.size() == 6, "quad mesh should triangulate to six expanded vertices");
  Require(mesh.geometry.indices.size() == 6, "quad mesh should produce six indices");
  Require(mesh.geometry.materialIndices.size() == 2, "quad mesh should produce two material indices");
  Require(Near(mesh.geometry.vertices[0].nrm.x, 1.0f) && Near(mesh.geometry.vertices[1].nrm.y, 1.0f) &&
              Near(mesh.geometry.vertices[2].nrm.z, 1.0f) && Near(mesh.geometry.vertices[5].nrm.x, -1.0f),
          "face-varying normals should follow original face corners through triangulation");
  Require(Near(mesh.geometry.vertices[0].texCoord.x, 0.0f) && Near(mesh.geometry.vertices[0].texCoord.y, 1.0f) &&
              Near(mesh.geometry.vertices[1].texCoord.x, 1.0f) && Near(mesh.geometry.vertices[1].texCoord.y, 1.0f) &&
              Near(mesh.geometry.vertices[2].texCoord.x, 1.0f) && Near(mesh.geometry.vertices[2].texCoord.y, 0.0f) &&
              Near(mesh.geometry.vertices[5].texCoord.x, 0.0f) && Near(mesh.geometry.vertices[5].texCoord.y, 0.0f),
          "authored primvars:st should be used and V-flipped through triangulation");
  Require(Near(mesh.worldTransform[3][0], 2.0f) && Near(mesh.worldTransform[3][1], 3.0f) &&
              Near(mesh.worldTransform[3][2], 4.0f),
          "USD matrix did not convert to engine translation");
  Require(mesh.materials.size() == 1, "expected one material on ground mesh");
  Require(mesh.materials[0].baseColorTextureId == 0, "UsdUVTexture diffuseColor should register base color texture");
  Require(Near(mesh.materials[0].baseColorFactor.x, 1.0f) && Near(mesh.materials[0].baseColorFactor.y, 1.0f) &&
              Near(mesh.materials[0].baseColorFactor.z, 1.0f),
          "texture-connected diffuseColor fallback should not darken base color texture");
  Require(Near(mesh.materials[0].metallicFactor, 0.25f), "UsdPreviewSurface metallic mismatch");
  Require(Near(mesh.materials[0].roughnessFactor, 0.33f), "UsdPreviewSurface roughness mismatch");
  Require(Near(mesh.materials[0].opacityFactor, 0.75f), "UsdPreviewSurface opacity mismatch");
  Require(Near(mesh.materials[0].emissionFactor.x, 0.1f) && Near(mesh.materials[0].emissionFactor.y, 0.2f) &&
              Near(mesh.materials[0].emissionFactor.z, 0.3f),
          "UsdPreviewSurface emissiveColor mismatch");

  const headless_training::TrainingMeshInstance& displayColorMesh = scene.meshes[1];
  Require(displayColorMesh.materials.size() == 1, "expected displayColor fallback material");
  Require(Near(displayColorMesh.materials[0].baseColorFactor.x, 0.4f) &&
              Near(displayColorMesh.materials[0].baseColorFactor.y, 0.5f) &&
              Near(displayColorMesh.materials[0].baseColorFactor.z, 0.6f),
          "displayColor should become fallback material base color");
  Require(Near(displayColorMesh.materials[0].opacityFactor, 0.7f),
          "displayOpacity should become fallback material opacity");

  Require(scene.textureAssets.size() == 2, "expected material and dome texture assets");
  Require(scene.textureAssets[0].usage == TextureUsage::BaseColor, "first texture should be base color");
  Require(scene.textureAssets[1].usage == TextureUsage::Light, "second texture should be dome light");
  Require(!scene.textureAssets[0].encodedBytes.empty() && !scene.textureAssets[1].encodedBytes.empty(),
          "texture assets should export encoded bytes");
  Require(scene.lights.size() == 2, "expected dome and sphere light");
  Require(scene.lights[0].type == 2, "dome light type mismatch");
  Require(scene.lights[0].textureID == 1, "dome light should reference light texture");
  if(!(Near(scene.lights[0].baseEmission.x, 0.4f) && Near(scene.lights[0].baseEmission.y, 0.6f) &&
       Near(scene.lights[0].baseEmission.z, 0.8f)))
  {
    std::cerr << "actual dome base emission: " << scene.lights[0].baseEmission.x << ", "
              << scene.lights[0].baseEmission.y << ", " << scene.lights[0].baseEmission.z << '\n';
    Require(false, "dome light base emission mismatch");
  }
  Require(scene.lights[1].type == 0, "sphere light type mismatch");
  Require(Near(scene.lights[1].position.x, 5.0f) && Near(scene.lights[1].position.y, 6.0f) &&
              Near(scene.lights[1].position.z, 7.0f),
          "sphere light transform mismatch");
  Require(Near(scene.lights[1].radius, 2.0f), "sphere light radius mismatch");

  Require(scene.cameras.size() == 2, "expected two cameras");
  Require(scene.cameras[0].name == "/World/MainCamera", "camera path mismatch");
  Require(Near(scene.cameras[0].position.x, 0.0f) && Near(scene.cameras[0].position.y, 1.0f) &&
              Near(scene.cameras[0].position.z, 2.0f),
          "camera transform position mismatch");
  Require(Near(scene.cameras[0].forward.x, 0.0f) && Near(scene.cameras[0].forward.y, 0.0f) &&
              Near(scene.cameras[0].forward.z, -1.0f),
          "camera forward should use local -Z");
  Require(scene.cameras[1].name == "/World/RearCamera", "second camera path mismatch");

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
  options.cameraPath = "/World/RearCamera";
  const std::filesystem::path selectedFixture = WriteFixture();
  const headless_training::TrainingSceneDescription selectedCameraScene =
      headless_training::LoadUsdTrainingScene(selectedFixture, options);
  std::filesystem::remove(selectedFixture);
  std::filesystem::remove(std::filesystem::temp_directory_path() /
                          "headless_training_usd_scene_loader_test_texture.png");
  const auto selectedCameraIt =
      std::find_if(selectedCameraScene.cameras.begin(), selectedCameraScene.cameras.end(), [](const CameraSpec& camera) {
        return camera.name == "/World/RearCamera";
      });
  Require(selectedCameraScene.cameras.size() == 2, "camera path should not filter loaded cameras");
  Require(selectedCameraIt != selectedCameraScene.cameras.end(), "camera path selection failed");
  Require(Near(selectedCameraIt->position.x, 3.0f) && Near(selectedCameraIt->position.y, 4.0f) &&
              Near(selectedCameraIt->position.z, 5.0f),
          "selected camera transform mismatch");

  options.cameraPath = "/World/MissingCamera";
  const std::filesystem::path missingCameraFixture = WriteFixture();
  bool missingCameraRejected = false;
  try
  {
    (void)headless_training::LoadUsdTrainingScene(missingCameraFixture, options);
  }
  catch(const std::runtime_error& error)
  {
    missingCameraRejected = std::string(error.what()).find("requested camera path") != std::string::npos;
  }
  std::filesystem::remove(missingCameraFixture);
  std::filesystem::remove(std::filesystem::temp_directory_path() /
                          "headless_training_usd_scene_loader_test_texture.png");
  Require(missingCameraRejected, "missing requested camera path should fail clearly");

  const std::filesystem::path startTimeFixture = WriteStartTimeFixture();
  const headless_training::TrainingSceneDescription startTimeScene =
      headless_training::LoadUsdTrainingScene(startTimeFixture);
  std::filesystem::remove(startTimeFixture);
  Require(startTimeScene.meshes.size() == 1, "expected one mesh in start-time fixture");
  Require(Near(startTimeScene.meshes[0].worldTransform[3][0], 1.0f) &&
              Near(startTimeScene.meshes[0].worldTransform[3][1], 2.0f) &&
              Near(startTimeScene.meshes[0].worldTransform[3][2], 3.0f),
          "USD loader should sample transforms at stage start time instead of default time");

  return 0;
}
