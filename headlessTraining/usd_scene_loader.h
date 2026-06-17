#pragma once

#include "training_scene.h"

#include <filesystem>
#include <string>

namespace headless_training
{

struct UsdSceneLoadOptions
{
  std::string cameraPath;
};

TrainingSceneDescription LoadUsdTrainingScene(const std::filesystem::path& usdPath,
                                              const UsdSceneLoadOptions& options = {});

} // namespace headless_training
