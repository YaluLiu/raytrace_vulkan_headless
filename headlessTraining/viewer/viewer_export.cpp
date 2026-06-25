#include "viewer_export.h"

#include "output_writers.h"

#include <sstream>
#include <stdexcept>

namespace headless_training
{
namespace
{
std::filesystem::path MakeUniquePath(std::filesystem::path path)
{
  if(!std::filesystem::exists(path))
  {
    return path;
  }

  const std::filesystem::path parent = path.parent_path();
  const std::string stem = path.stem().string();
  const std::string extension = path.extension().string();
  for(int suffix = 1; suffix < 10000; ++suffix)
  {
    std::ostringstream name;
    name << stem << "_" << suffix;
    std::filesystem::path candidate = parent / (name.str() + extension);
    if(!std::filesystem::exists(candidate))
    {
      return candidate;
    }
  }
  return path;
}

void ThrowIfWriteFailed(bool ok, const std::string& errorMessage, const std::filesystem::path& path)
{
  if(!ok)
  {
    throw std::runtime_error(errorMessage.empty() ? "failed to write " + path.string() : errorMessage);
  }
}
} // namespace

std::filesystem::path MakeUniqueViewerDebugPath(const ViewerExportSettings& settings,
                                                const std::string& prefix,
                                                uint64_t frameIndex,
                                                const std::string& extension)
{
  return MakeUniquePath(settings.outputDir / "viewer_debug" / FormatFrameFileName(prefix, frameIndex, extension));
}

std::string ExportViewerScreenshot(Engine& engine, const ViewerExportSettings& settings, uint64_t frameIndex)
{
  const std::filesystem::path path = MakeUniqueViewerDebugPath(settings, "frame", frameIndex, ".png");
  std::filesystem::create_directories(path.parent_path());
  engine.saveFrame(path.string());
  return "saved " + path.string();
}

std::string ExportViewerLidarCsv(Engine& engine, const ViewerExportSettings& settings, uint64_t frameIndex)
{
  const std::filesystem::path path = MakeUniqueViewerDebugPath(settings, "lidar_frame", frameIndex, ".csv");
  std::filesystem::create_directories(path.parent_path());
  std::string error;
  ThrowIfWriteFailed(WriteLidarCsv(engine.readLidarPointCloudFrame(), frameIndex, path, &error), error, path);
  return "saved " + path.string();
}

std::string ExportViewerHeightScanCsv(Engine& engine, const ViewerExportSettings& settings, uint64_t frameIndex)
{
  const std::filesystem::path path = MakeUniqueViewerDebugPath(settings, "height_scan_frame", frameIndex, ".csv");
  std::filesystem::create_directories(path.parent_path());
  std::string error;
  ThrowIfWriteFailed(WriteHeightScanCsv(engine.readHeightScanFrame(), frameIndex, path, &error), error, path);
  return "saved " + path.string();
}

} // namespace headless_training
