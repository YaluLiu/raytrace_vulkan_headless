#pragma once

#include <filesystem>
#include <string>

namespace headless_training
{

struct CliOptions
{
  std::filesystem::path usdPath;
  std::filesystem::path outputDir;
  std::filesystem::path physicsReplayPath;
  std::string cameraPath;
  std::string pluginSearchRoot;
  int frames{1};
  int width{1280};
  int height{720};
  bool savePreview{false};
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
