#include "output_writers.h"
#include "usd_animation_source.h"
#include "usd_scene_loader.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

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

std::string ReadFile(const std::filesystem::path& path)
{
  std::ifstream input(path);
  std::ostringstream content;
  content << input.rdbuf();
  return content.str();
}

std::filesystem::path WriteAnimatedUsdFixture(const std::filesystem::path& tempDir)
{
  const std::filesystem::path usdPath = tempDir / "animated_scene.usda";
  std::ofstream usd(usdPath, std::ios::out | std::ios::trunc);
  usd << R"USD(#usda 1.0
(
    startTimeCode = 0
    endTimeCode = 10
)

def Xform "World"
{
    double3 xformOp:translate.timeSamples = {
        0: (1, 2, 3),
        10: (4, 5, 6),
    }
    uniform token[] xformOpOrder = ["xformOp:translate"]

    def Mesh "RobotMesh"
    {
        point3f[] points = [(0, 0, 0), (1, 0, 0), (0, 1, 0)]
        int[] faceVertexCounts = [3]
        int[] faceVertexIndices = [0, 1, 2]
    }
}
)USD";
  return usdPath;
}
} // namespace

int main()
{
  const std::filesystem::path tempDir =
      std::filesystem::temp_directory_path() / "headless_training_usd_animation_output_test";
  std::filesystem::remove_all(tempDir);
  std::filesystem::create_directories(tempDir);

  const std::filesystem::path usdPath = WriteAnimatedUsdFixture(tempDir);
  const headless_training::TrainingSceneDescription scene = headless_training::LoadUsdTrainingScene(usdPath);
  std::unique_ptr<headless_training::AnimationStateSource> source =
      headless_training::LoadUsdAnimationSource(usdPath, scene);
  Require(source != nullptr, "expected USD animation source for sampled xform");
  std::vector<headless_training::InstancePoseUpdate> updates;
  Require(source->nextFrame(updates), "expected first USD animation frame");
  Require(updates.size() == 1, "expected one USD animation update");
  Require(updates[0].name == "/World/RobotMesh", "USD animation update name mismatch");
  Require(updates[0].visible, "USD animation visibility mismatch");
  Require(Near(updates[0].worldTransform[3][0], 1.0f) && Near(updates[0].worldTransform[3][1], 2.0f) &&
              Near(updates[0].worldTransform[3][2], 3.0f),
          "first USD animation translation mismatch");
  Require(source->nextFrame(updates), "expected second USD animation frame");
  Require(updates.size() == 1, "expected one second-frame USD animation update");
  Require(Near(updates[0].worldTransform[3][0], 4.0f) && Near(updates[0].worldTransform[3][1], 5.0f) &&
              Near(updates[0].worldTransform[3][2], 6.0f),
          "second USD animation translation mismatch");
  Require(!source->nextFrame(updates), "USD animation source should report exhaustion");

  LidarFramePointCloud lidarFrame;
  lidarFrame.frameId = 11;
  LidarSensorPointCloud lidarSensor;
  lidarSensor.name = "front,lidar";
  lidarSensor.sensorIndex = 3;
  lidarSensor.width = 1;
  lidarSensor.height = 1;
  LidarPoint point;
  point.positionWs = glm::vec3(1.0f, 2.0f, 3.0f);
  point.rangeMeters = 4.0f;
  point.sensorIndex = 3;
  point.beamIndex = 5;
  point.flags = LidarPointFlagValid | LidarPointFlagHit;
  point.intensity = 0.5f;
  lidarSensor.points.push_back(point);
  lidarFrame.sensors.push_back(lidarSensor);

  HeightScanFrame heightFrame;
  heightFrame.frameId = 11;
  HeightScanSensorGrid heightSensor;
  heightSensor.name = "feet";
  heightSensor.sensorIndex = 1;
  heightSensor.width = 1;
  heightSensor.height = 1;
  HeightScanSample sample;
  sample.positionWs = glm::vec3(4.0f, 5.0f, 6.0f);
  sample.distanceMeters = 7.0f;
  sample.sensorIndex = 1;
  sample.uIndex = 8;
  sample.vIndex = 9;
  sample.flags = HeightScanSampleFlagValid | HeightScanSampleFlagOutOfRange;
  heightSensor.samples.push_back(sample);
  heightFrame.sensors.push_back(heightSensor);

  std::string error;
  const std::filesystem::path lidarCsv = tempDir / "lidar.csv";
  Require(headless_training::WriteLidarCsv(lidarFrame, 0, lidarCsv, &error), error.c_str());
  const std::string lidarContent = ReadFile(lidarCsv);
  Require(lidarContent.find("\"front,lidar\"") != std::string::npos, "lidar CSV should quote sensor names");
  Require(lidarContent.find(",5,1,2,3,4,0.5,3,1,1,0") != std::string::npos, "lidar CSV row mismatch");

  const std::filesystem::path heightCsv = tempDir / "height.csv";
  Require(headless_training::WriteHeightScanCsv(heightFrame, 0, heightCsv, &error), error.c_str());
  const std::string heightContent = ReadFile(heightCsv);
  Require(heightContent.find("0,feet,1,1,1,0,8,9,4,5,6,7,5,1,0,1") != std::string::npos,
          "height scan CSV row mismatch");

  headless_training::TrainingManifest manifest;
  manifest.usdPath = std::string("scene") + static_cast<char>(1) + ".usda";
  manifest.width = 64;
  manifest.height = 32;
  manifest.requestedFrames = 1;
  manifest.renderedFrames = 1;
  headless_training::FrameOutputManifest frame;
  frame.frameIndex = 0;
  frame.lidarCsv = "lidar/frame_000000.csv";
  frame.heightScanCsv = "height_scan/frame_000000.csv";
  frame.lidarPointCount = headless_training::CountLidarPoints(lidarFrame);
  frame.heightScanSampleCount = headless_training::CountHeightScanSamples(heightFrame);
  manifest.frames.push_back(frame);
  const std::filesystem::path manifestPath = tempDir / "manifest.json";
  Require(headless_training::WriteManifestJson(manifest, manifestPath, &error), error.c_str());
  const std::string manifestContent = ReadFile(manifestPath);
  Require(manifestContent.find("\"rendered_frames\": 1") != std::string::npos, "manifest rendered frame mismatch");
  Require(manifestContent.find("\"lidar_point_count\": 1") != std::string::npos, "manifest lidar count mismatch");
  Require(manifestContent.find("scene\\u0001.usda") != std::string::npos,
          "manifest JSON should escape control characters");

  std::filesystem::remove_all(tempDir);
  return 0;
}
