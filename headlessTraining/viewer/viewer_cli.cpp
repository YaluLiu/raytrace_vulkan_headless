#include "viewer_cli.h"

#include <cstdlib>
#include <cstring>
#include <limits>

namespace headless_training
{
namespace
{
bool IsOption(const char* arg, const char* name)
{
  return std::strcmp(arg, name) == 0;
}

bool ReadValue(int& index, int argc, char** argv, std::string& value, std::string& error)
{
  if(index + 1 >= argc)
  {
    error = std::string("missing value for ") + argv[index];
    return false;
  }
  value = argv[++index];
  return true;
}

bool ParsePositiveInt(const std::string& text, int& value)
{
  char* end = nullptr;
  const long parsed = std::strtol(text.c_str(), &end, 10);
  if(end == text.c_str() || *end != '\0' || parsed <= 0 || parsed > std::numeric_limits<int>::max())
  {
    return false;
  }
  value = static_cast<int>(parsed);
  return true;
}
} // namespace

ViewerCliParseResult ParseViewerCommandLine(int argc, char** argv)
{
  ViewerCliParseResult result;
  result.ok = true;

  for(int i = 1; i < argc; ++i)
  {
    const char* arg = argv[i];
    std::string value;
    if(IsOption(arg, "--help") || IsOption(arg, "-h"))
    {
      result.options.showHelp = true;
    }
    else if(IsOption(arg, "--usd"))
    {
      if(!ReadValue(i, argc, argv, value, result.error))
      {
        result.ok = false;
        return result;
      }
      result.options.usdPath = value;
    }
    else if(IsOption(arg, "--width") || IsOption(arg, "--height"))
    {
      if(!ReadValue(i, argc, argv, value, result.error))
      {
        result.ok = false;
        return result;
      }
      int parsed = 0;
      if(!ParsePositiveInt(value, parsed))
      {
        result.ok = false;
        result.error = std::string("invalid positive integer for ") + arg + ": " + value;
        return result;
      }
      if(IsOption(arg, "--width"))
      {
        result.options.width = parsed;
      }
      else
      {
        result.options.height = parsed;
      }
    }
    else if(IsOption(arg, "--camera"))
    {
      if(!ReadValue(i, argc, argv, value, result.error))
      {
        result.ok = false;
        return result;
      }
      result.options.cameraPath = value;
    }
    else if(IsOption(arg, "--plugin-search-root"))
    {
      if(!ReadValue(i, argc, argv, value, result.error))
      {
        result.ok = false;
        return result;
      }
      result.options.pluginSearchRoot = value;
    }
    else if(IsOption(arg, "--output-dir"))
    {
      if(!ReadValue(i, argc, argv, value, result.error))
      {
        result.ok = false;
        return result;
      }
      result.options.outputDir = value;
    }
    else if(IsOption(arg, "--enable-lidar"))
    {
      result.options.enableLidar = true;
    }
    else if(IsOption(arg, "--disable-lidar"))
    {
      result.options.enableLidar = false;
    }
    else if(IsOption(arg, "--enable-height-scan"))
    {
      result.options.enableHeightScan = true;
    }
    else if(IsOption(arg, "--disable-height-scan"))
    {
      result.options.enableHeightScan = false;
    }
    else
    {
      result.ok = false;
      result.error = std::string("unknown option: ") + arg;
      return result;
    }
  }

  if(!result.options.showHelp && result.options.usdPath.empty())
  {
    result.ok = false;
    result.error = "missing required --usd <path>";
  }
  return result;
}

std::string BuildViewerHelpText()
{
  return "Usage: robot_training_viewer --usd <path> [options]\n\n"
         "Options:\n"
         "  --width <pixels>             Window and render width (default: 1280)\n"
         "  --height <pixels>            Window and render height (default: 720)\n"
         "  --camera <usd-path>          Optional startup USD camera path\n"
         "  --plugin-search-root <path>  Optional shader/plugin search root\n"
         "  --output-dir <path>          Optional directory for explicit debug exports\n"
         "  --enable-lidar               Enable LiDAR compute at startup\n"
         "  --disable-lidar              Disable LiDAR compute at startup\n"
         "  --enable-height-scan         Enable height scan compute at startup\n"
         "  --disable-height-scan        Disable height scan compute at startup\n"
         "  --help                       Show this help\n";
}

} // namespace headless_training
