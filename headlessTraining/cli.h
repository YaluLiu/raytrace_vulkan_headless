#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <glm/glm.hpp>

namespace headless_training
{

struct CliOptions
{
  std::filesystem::path usdPath;
  std::filesystem::path outputDir;
  std::string cameraPath;
  std::string pluginSearchRoot;
  std::optional<glm::vec3> previewCameraPosition;
  std::optional<glm::vec3> previewCameraTarget;
  std::optional<float> previewCameraFovDegrees;
  std::optional<float> previewCameraDistanceScale;
  int frames{1};
  int width{1280};
  int height{720};
  bool savePreview{false};
  bool previewLidarPoints{false};
  bool previewHeightScanPoints{false};
  bool exportLidar{true};
  bool exportHeightScan{true};
  bool showHelp{false};
};

struct CliParseResult
{
  CliOptions options;
  bool ok{false};
  std::string error;
};

CliParseResult ParseCommandLine(int argc, char** argv);
std::string BuildHelpText();

} // namespace headless_training
