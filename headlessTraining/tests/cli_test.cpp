#include "cli.h"

#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
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

bool Near(float lhs, float rhs)
{
  return std::fabs(lhs - rhs) < 1.0e-4f;
}

headless_training::CliParseResult Parse(std::vector<std::string> args)
{
  std::vector<char*> argv;
  argv.reserve(args.size());
  for(std::string& arg : args)
  {
    argv.push_back(arg.data());
  }
  return headless_training::ParseCommandLine(static_cast<int>(argv.size()), argv.data());
}
} // namespace

int main()
{
  const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "headless_training_cli_test";
  std::filesystem::remove_all(tempDir);
  std::filesystem::create_directories(tempDir);
  const std::filesystem::path usdPath = tempDir / "scene.usda";
  {
    std::ofstream usd(usdPath, std::ios::out | std::ios::trunc);
    usd << "#usda 1.0\n";
  }

  {
    headless_training::CliParseResult result =
        Parse({"robot_training_headless", "--usd", usdPath.string(), "--output-dir", (tempDir / "out").string(),
               "--frames", "3", "--width", "64", "--height", "32", "--no-export-lidar", "--save-preview",
               "--preview-camera-position", "1.5,2.5,-3.5", "--preview-camera-target", "0,0.25,1",
               "--preview-camera-fov", "60", "--preview-camera-distance-scale", "1.8", "--preview-lidar-points",
               "--preview-height-scan-points"});
    Require(result.ok, result.error.c_str());
    Require(result.options.frames == 3, "frames parse mismatch");
    Require(result.options.width == 64 && result.options.height == 32, "render size parse mismatch");
    Require(!result.options.exportLidar, "lidar toggle parse mismatch");
    Require(result.options.exportHeightScan, "height scan default should stay enabled");
    Require(result.options.savePreview, "preview toggle parse mismatch");
    Require(result.options.previewLidarPoints, "preview lidar point overlay parse mismatch");
    Require(result.options.previewHeightScanPoints, "preview height scan point overlay parse mismatch");
    Require(result.options.previewCameraPosition.has_value(), "preview camera position should parse");
    Require(Near(result.options.previewCameraPosition->x, 1.5f) &&
                Near(result.options.previewCameraPosition->y, 2.5f) &&
                Near(result.options.previewCameraPosition->z, -3.5f),
            "preview camera position parse mismatch");
    Require(result.options.previewCameraTarget.has_value(), "preview camera target should parse");
    Require(Near(result.options.previewCameraTarget->x, 0.0f) &&
                Near(result.options.previewCameraTarget->y, 0.25f) &&
                Near(result.options.previewCameraTarget->z, 1.0f),
            "preview camera target parse mismatch");
    Require(result.options.previewCameraFovDegrees.has_value() && Near(*result.options.previewCameraFovDegrees, 60.0f),
            "preview camera fov parse mismatch");
    Require(result.options.previewCameraDistanceScale.has_value() &&
                Near(*result.options.previewCameraDistanceScale, 1.8f),
            "preview camera distance scale parse mismatch");
    Require(std::filesystem::exists(tempDir / "out"), "output directory should be created");
  }

  {
    headless_training::CliParseResult result =
        Parse({"robot_training_headless", "--usd", usdPath.string(), "--output-dir", (tempDir / "out1b").string(),
               "--preview-lidar-points", "--preview-height-scan-points", "--no-preview-lidar-points",
               "--no-preview-height-scan-points"});
    Require(result.ok, result.error.c_str());
    Require(!result.options.previewLidarPoints, "preview lidar point overlay should be disableable");
    Require(!result.options.previewHeightScanPoints, "preview height scan point overlay should be disableable");
  }

  {
    headless_training::CliParseResult result =
        Parse({"robot_training_headless", "--output-dir", (tempDir / "out2").string()});
    Require(!result.ok && result.error.find("--usd") != std::string::npos, "missing --usd should fail clearly");
  }

  {
    headless_training::CliParseResult result =
        Parse({"robot_training_headless", "--usd", "--output-dir", (tempDir / "out3").string()});
    Require(!result.ok && result.error.find("requires a value") != std::string::npos,
            "missing option value should fail clearly");
  }

  {
    headless_training::CliParseResult result =
        Parse({"robot_training_headless", "--usd", usdPath.string(), "--output-dir", (tempDir / "out4").string(),
               "--unknown"});
    Require(!result.ok && result.error.find("unknown option") != std::string::npos, "unknown option should fail");
  }

  {
    headless_training::CliParseResult result =
        Parse({"robot_training_headless", "--usd", usdPath.string(), "--output-dir", (tempDir / "out5").string(),
               "--preview-camera-position", "1,2"});
    Require(!result.ok && result.error.find("--preview-camera-position") != std::string::npos,
            "bad preview camera position should fail clearly");
  }

  {
    headless_training::CliParseResult result =
        Parse({"robot_training_headless", "--usd", usdPath.string(), "--output-dir", (tempDir / "out6").string(),
               "--preview-camera-fov", "180"});
    Require(!result.ok && result.error.find("--preview-camera-fov") != std::string::npos,
            "bad preview camera fov should fail clearly");
  }

  {
    headless_training::CliParseResult result =
        Parse({"robot_training_headless", "--usd", usdPath.string(), "--output-dir", (tempDir / "out7").string(),
               "--preview-camera-distance-scale", "0"});
    Require(!result.ok && result.error.find("--preview-camera-distance-scale") != std::string::npos,
            "bad preview camera distance scale should fail clearly");
  }

  {
    headless_training::CliParseResult result = Parse({"robot_training_headless", "--help"});
    Require(result.ok && result.options.showHelp, "--help should parse without required paths");
  }

  std::filesystem::remove_all(tempDir);
  return 0;
}
