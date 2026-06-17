#include "cli.h"

#include <charconv>
#include <filesystem>
#include <sstream>
#include <string_view>

namespace headless_training
{
namespace
{
bool ParsePositiveInt(std::string_view text, int* value)
{
  if(text.empty() || value == nullptr)
  {
    return false;
  }

  int parsed = 0;
  const auto [ptr, ec] = std::from_chars(text.data(), text.data() + text.size(), parsed);
  if(ec != std::errc() || ptr != text.data() + text.size() || parsed <= 0)
  {
    return false;
  }

  *value = parsed;
  return true;
}

bool NeedsValue(std::string_view arg)
{
  return arg == "--usd" || arg == "--output-dir" || arg == "--frames" || arg == "--width" || arg == "--height" ||
         arg == "--physics-replay" || arg == "--camera" || arg == "--plugin-search-root";
}

bool ReadValue(int argc, char** argv, int* index, std::string* value, std::string* error)
{
  if(index == nullptr || value == nullptr || error == nullptr)
  {
    return false;
  }

  const std::string option = argv[*index];
  if(*index + 1 >= argc)
  {
    *error = option + " requires a value";
    return false;
  }

  const std::string next = argv[*index + 1];
  if(next.rfind("--", 0) == 0 && NeedsValue(next))
  {
    *error = option + " requires a value";
    return false;
  }

  *value = next;
  ++(*index);
  return true;
}
} // namespace

std::string BuildHelpText()
{
  return
      "Usage: robot_training_headless --usd <path> --output-dir <path> [options]\n"
      "\n"
      "Options:\n"
      "  --usd <path>                  Training scene USD file.\n"
      "  --output-dir <path>           Directory for manifest, sensor CSV, and preview outputs.\n"
      "  --frames <N>                  Number of frames to render. Default: 1.\n"
      "  --width <pixels>              Render width. Default: 1280.\n"
      "  --height <pixels>             Render height. Default: 720.\n"
      "  --physics-replay <path>       Optional frame-by-frame instance transform JSON replay.\n"
      "  --camera <usd-path>           Optional USD camera prim path to use as the main camera.\n"
      "  --plugin-search-root <path>   Optional engine resource/plugin search root.\n"
      "  --save-preview                Save preview PNG for each frame.\n"
      "  --export-lidar                Enable LiDAR CSV export. Default: enabled.\n"
      "  --no-export-lidar             Disable LiDAR CSV export.\n"
      "  --export-height-scan          Enable height scan CSV export. Default: enabled.\n"
      "  --no-export-height-scan       Disable height scan CSV export.\n"
      "  --help                        Print this help text.\n";
}

CliParseResult ParseCommandLine(int argc, char** argv)
{
  CliParseResult result;
  result.options = CliOptions{};

  for(int i = 1; i < argc; ++i)
  {
    const std::string arg = argv[i];
    std::string value;

    if(arg == "--help" || arg == "-h")
    {
      result.options.showHelp = true;
      result.ok = true;
      return result;
    }
    if(arg == "--usd")
    {
      if(!ReadValue(argc, argv, &i, &value, &result.error))
      {
        return result;
      }
      result.options.usdPath = value;
    }
    else if(arg == "--output-dir")
    {
      if(!ReadValue(argc, argv, &i, &value, &result.error))
      {
        return result;
      }
      result.options.outputDir = value;
    }
    else if(arg == "--frames")
    {
      if(!ReadValue(argc, argv, &i, &value, &result.error) || !ParsePositiveInt(value, &result.options.frames))
      {
        result.error = "expected positive integer for --frames";
        return result;
      }
    }
    else if(arg == "--width")
    {
      if(!ReadValue(argc, argv, &i, &value, &result.error) || !ParsePositiveInt(value, &result.options.width))
      {
        result.error = "expected positive integer for --width";
        return result;
      }
    }
    else if(arg == "--height")
    {
      if(!ReadValue(argc, argv, &i, &value, &result.error) || !ParsePositiveInt(value, &result.options.height))
      {
        result.error = "expected positive integer for --height";
        return result;
      }
    }
    else if(arg == "--physics-replay")
    {
      if(!ReadValue(argc, argv, &i, &value, &result.error))
      {
        return result;
      }
      result.options.physicsReplayPath = value;
    }
    else if(arg == "--camera")
    {
      if(!ReadValue(argc, argv, &i, &value, &result.error))
      {
        return result;
      }
      result.options.cameraPath = value;
    }
    else if(arg == "--plugin-search-root")
    {
      if(!ReadValue(argc, argv, &i, &value, &result.error))
      {
        return result;
      }
      result.options.pluginSearchRoot = value;
    }
    else if(arg == "--save-preview")
    {
      result.options.savePreview = true;
    }
    else if(arg == "--export-lidar")
    {
      result.options.exportLidar = true;
    }
    else if(arg == "--no-export-lidar")
    {
      result.options.exportLidar = false;
    }
    else if(arg == "--export-height-scan")
    {
      result.options.exportHeightScan = true;
    }
    else if(arg == "--no-export-height-scan")
    {
      result.options.exportHeightScan = false;
    }
    else
    {
      result.error = "unknown option: " + arg;
      return result;
    }
  }

  if(result.options.usdPath.empty())
  {
    result.error = "missing required --usd <path>";
    return result;
  }
  if(result.options.outputDir.empty())
  {
    result.error = "missing required --output-dir <path>";
    return result;
  }
  if(!std::filesystem::exists(result.options.usdPath))
  {
    result.error = "USD file does not exist: " + result.options.usdPath.string();
    return result;
  }
  if(!result.options.physicsReplayPath.empty() && !std::filesystem::exists(result.options.physicsReplayPath))
  {
    result.error = "physics replay file does not exist: " + result.options.physicsReplayPath.string();
    return result;
  }

  std::error_code ec;
  std::filesystem::create_directories(result.options.outputDir, ec);
  if(ec)
  {
    result.error = "failed to create output directory " + result.options.outputDir.string() + ": " + ec.message();
    return result;
  }

  result.ok = true;
  return result;
}

} // namespace headless_training
