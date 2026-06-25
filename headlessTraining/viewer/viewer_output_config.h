#pragma once

#include "training_scene.h"

#include <engine/output_config.hpp>

#include <cstdint>
#include <filesystem>

namespace headless_training
{

enum class SensorDisplayMode
{
  All,
  Single,
};

struct SensorViewerSettings
{
  bool enableCompute{true};
  bool showOverlay{false};
  SensorDisplayMode displayMode{SensorDisplayMode::All};
  uint32_t sensorIndex{0};
  float pointSizePixels{2.0f};
};

struct ViewerExportSettings
{
  std::filesystem::path outputDir;
};

struct ViewerState
{
  CameraSpec visualCamera;
  SensorViewerSettings lidar;
  SensorViewerSettings heightScan;
  bool replayPaused{true};
  bool replayEnded{false};
  uint64_t frameIndex{0};
  ViewerExportSettings exportSettings;
};

RendererOutputConfig BuildViewerOutputConfig(const TrainingSceneDescription& scene, const ViewerState& state);
uint32_t ClampSensorIndex(uint32_t sensorIndex, size_t sensorCount);

} // namespace headless_training
