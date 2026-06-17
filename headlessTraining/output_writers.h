#pragma once

#include <engine/height_scan_types.hpp>
#include <engine/lidar_types.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace headless_training
{

struct FrameOutputManifest
{
  uint64_t frameIndex{0};
  std::string lidarCsv;
  std::string heightScanCsv;
  std::string previewPng;
  size_t lidarPointCount{0};
  size_t heightScanSampleCount{0};
};

struct TrainingManifest
{
  std::string usdPath;
  std::string physicsReplayPath;
  uint32_t width{0};
  uint32_t height{0};
  uint32_t requestedFrames{0};
  uint32_t renderedFrames{0};
  size_t meshCount{0};
  size_t cameraCount{0};
  size_t lidarSensorCount{0};
  size_t heightScanSensorCount{0};
  std::vector<FrameOutputManifest> frames;
};

size_t CountLidarPoints(const LidarFramePointCloud& frame);
size_t CountHeightScanSamples(const HeightScanFrame& frame);

bool WriteLidarCsv(const LidarFramePointCloud& frame,
                   uint64_t csvFrameId,
                   const std::filesystem::path& filePath,
                   std::string* errorMessage);
bool WriteHeightScanCsv(const HeightScanFrame& frame,
                        uint64_t csvFrameId,
                        const std::filesystem::path& filePath,
                        std::string* errorMessage);
bool WriteManifestJson(const TrainingManifest& manifest,
                       const std::filesystem::path& filePath,
                       std::string* errorMessage);

std::string FormatFrameFileName(const std::string& prefix, uint64_t frameIndex, const std::string& extension);

} // namespace headless_training
