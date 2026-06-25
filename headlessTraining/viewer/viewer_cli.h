#pragma once

#include <filesystem>
#include <string>

namespace headless_training
{

struct ViewerCliOptions
{
  std::filesystem::path usdPath;
  std::string cameraPath;
  std::string pluginSearchRoot;
  std::filesystem::path outputDir;
  int width{1280};
  int height{720};
  bool enableLidar{true};
  bool enableHeightScan{true};
  bool showHelp{false};
};

struct ViewerCliParseResult
{
  ViewerCliOptions options;
  bool ok{false};
  std::string error;
};

ViewerCliParseResult ParseViewerCommandLine(int argc, char** argv);
std::string BuildViewerHelpText();

} // namespace headless_training
