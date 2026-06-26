#pragma once

#include "physics_frame.h"
#include "training_scene.h"

#include <filesystem>
#include <memory>

namespace headless_training
{

std::unique_ptr<PhysicsFrameSource> LoadUsdPhysicsReplaySource(const std::filesystem::path& usdPath,
                                                               const TrainingSceneDescription& scene);

} // namespace headless_training
