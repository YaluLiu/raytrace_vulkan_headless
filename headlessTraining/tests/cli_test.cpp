#include "cli.h"

#include <cstdlib>
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
               "--frames", "3", "--width", "64", "--height", "32", "--no-export-lidar", "--save-preview"});
    Require(result.ok, result.error.c_str());
    Require(result.options.frames == 3, "frames parse mismatch");
    Require(result.options.width == 64 && result.options.height == 32, "render size parse mismatch");
    Require(!result.options.exportLidar, "lidar toggle parse mismatch");
    Require(result.options.exportHeightScan, "height scan default should stay enabled");
    Require(result.options.savePreview, "preview toggle parse mismatch");
    Require(std::filesystem::exists(tempDir / "out"), "output directory should be created");
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
    headless_training::CliParseResult result = Parse({"robot_training_headless", "--help"});
    Require(result.ok && result.options.showHelp, "--help should parse without required paths");
  }

  std::filesystem::remove_all(tempDir);
  return 0;
}
