#pragma once

#include "viewer_output_config.h"

#include <engine/engine.hpp>

#include <cstdint>
#include <filesystem>
#include <string>

namespace headless_training
{

std::filesystem::path MakeUniqueViewerDebugPath(const ViewerExportSettings& settings,
                                                const std::string& prefix,
                                                uint64_t frameIndex,
                                                const std::string& extension);
std::string ExportViewerScreenshot(Engine& engine, const ViewerExportSettings& settings, uint64_t frameIndex);
std::string ExportViewerLidarCsv(Engine& engine, const ViewerExportSettings& settings, uint64_t frameIndex);
std::string ExportViewerHeightScanCsv(Engine& engine, const ViewerExportSettings& settings, uint64_t frameIndex);

} // namespace headless_training
