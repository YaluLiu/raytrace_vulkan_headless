#include "training_runner.h"

#include "output_writers.h"
#include "preview_camera.h"
#include "training_scene.h"
#include "usd_animation_source.h"
#include "usd_scene_loader.h"

#include <engine/engine.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace
{
void ThrowIfWriteFailed(bool ok, const std::string& errorMessage, const std::filesystem::path& path)
{
  if(ok)
  {
    return;
  }
  throw std::runtime_error(errorMessage.empty() ? "failed to write " + path.string() : errorMessage);
}

std::string RelativeOutputPath(const std::filesystem::path& outputDir, const std::filesystem::path& path)
{
  std::error_code ec;
  const std::filesystem::path relative = std::filesystem::relative(path, outputDir, ec);
  return ec ? path.string() : relative.generic_string();
}

bool HasPreviewCameraOverrides(const headless_training::CliOptions& options)
{
  return options.previewCameraPosition.has_value() || options.previewCameraTarget.has_value() ||
         options.previewCameraFovDegrees.has_value() || options.previewCameraDistanceScale.has_value();
}

headless_training::PreviewCameraOptions MakePreviewCameraOptions(const headless_training::CliOptions& options)
{
  headless_training::PreviewCameraOptions result;
  result.position = options.previewCameraPosition;
  result.target = options.previewCameraTarget;
  result.verticalFovDegrees = options.previewCameraFovDegrees;
  result.distanceScale = options.previewCameraDistanceScale;
  return result;
}

CameraSpec SelectMainCamera(const headless_training::TrainingSceneDescription& scene,
                            const headless_training::CliOptions& options)
{
  if(!HasPreviewCameraOverrides(options) && !options.cameraPath.empty())
  {
    const auto cameraIt = std::find_if(scene.cameras.begin(), scene.cameras.end(), [&](const CameraSpec& camera) {
      return camera.name == options.cameraPath;
    });
    if(cameraIt != scene.cameras.end())
    {
      return *cameraIt;
    }
  }
  return headless_training::BuildPreviewCamera(scene, MakePreviewCameraOptions(options));
}
} // namespace

namespace headless_training
{

void RunTraining(const CliOptions& options)
{
  UsdSceneLoadOptions loadOptions;
  if(!HasPreviewCameraOverrides(options))
  {
    loadOptions.cameraPath = options.cameraPath;
  }

  TrainingSceneDescription scene = LoadUsdTrainingScene(options.usdPath, loadOptions);
  TrainingSceneRuntime runtime(std::move(scene));
  std::unique_ptr<AnimationStateSource> animationSource = LoadUsdAnimationSource(options.usdPath, runtime.scene());

  Engine engine;
  if(!options.pluginSearchRoot.empty())
  {
    engine.setPluginSearchRoot(options.pluginSearchRoot);
  }
  engine.setup(options.width, options.height);
  runtime.uploadToEngine(engine);
  const CameraSpec mainCamera = SelectMainCamera(runtime.scene(), options);
  runtime.configureEngineOutputs(engine, options.exportLidar, options.exportHeightScan, mainCamera,
                                 options.previewLidarPoints, options.previewHeightScanPoints);
  engine.createRenderResources();

  TrainingManifest manifest;
  manifest.usdPath = options.usdPath.string();
  manifest.width = static_cast<uint32_t>(options.width);
  manifest.height = static_cast<uint32_t>(options.height);
  manifest.requestedFrames = static_cast<uint32_t>(options.frames);
  manifest.meshCount = runtime.scene().meshes.size();
  manifest.cameraCount = runtime.scene().cameras.size();
  manifest.lidarSensorCount = runtime.scene().lidarSensors.size();
  manifest.heightScanSensorCount = runtime.scene().heightScanSensors.size();

  for(int frameIndex = 0; frameIndex < options.frames; ++frameIndex)
  {
    AnimationFrameUpdates poseUpdates;
    if(animationSource && !animationSource->nextFrame(poseUpdates))
    {
      std::cerr << "[robot_training_headless] Warning: USD animation ended before frame " << frameIndex
                << "; reusing current scene transforms\n";
    }
    if(runtime.applyPoseUpdates(engine, poseUpdates))
    {
      runtime.configureEngineOutputs(engine, options.exportLidar, options.exportHeightScan, mainCamera,
                                     options.previewLidarPoints, options.previewHeightScanPoints);
    }

    engine.render();

    FrameOutputManifest frameOutput;
    frameOutput.frameIndex = static_cast<uint64_t>(frameIndex);

    std::string errorMessage;
    if(options.exportLidar)
    {
      const LidarFramePointCloud lidarFrame = engine.readLidarPointCloudFrame();
      const std::filesystem::path lidarPath =
          options.outputDir / "lidar" / FormatFrameFileName("frame", frameOutput.frameIndex, ".csv");
      ThrowIfWriteFailed(WriteLidarCsv(lidarFrame, frameOutput.frameIndex, lidarPath, &errorMessage), errorMessage,
                         lidarPath);
      frameOutput.lidarCsv = RelativeOutputPath(options.outputDir, lidarPath);
      frameOutput.lidarPointCount = CountLidarPoints(lidarFrame);
    }

    if(options.exportHeightScan)
    {
      const HeightScanFrame heightScanFrame = engine.readHeightScanFrame();
      const std::filesystem::path heightScanPath =
          options.outputDir / "height_scan" / FormatFrameFileName("frame", frameOutput.frameIndex, ".csv");
      errorMessage.clear();
      ThrowIfWriteFailed(WriteHeightScanCsv(heightScanFrame, frameOutput.frameIndex, heightScanPath, &errorMessage),
                         errorMessage, heightScanPath);
      frameOutput.heightScanCsv = RelativeOutputPath(options.outputDir, heightScanPath);
      frameOutput.heightScanSampleCount = CountHeightScanSamples(heightScanFrame);
    }

    if(options.savePreview)
    {
      const std::filesystem::path previewPath =
          options.outputDir / "preview" / FormatFrameFileName("frame", frameOutput.frameIndex, ".png");
      std::filesystem::create_directories(previewPath.parent_path());
      engine.saveFrame(previewPath.string());
      frameOutput.previewPng = RelativeOutputPath(options.outputDir, previewPath);
    }

    manifest.frames.push_back(frameOutput);
    manifest.renderedFrames = static_cast<uint32_t>(manifest.frames.size());
  }

  std::string errorMessage;
  const std::filesystem::path manifestPath = options.outputDir / "manifest.json";
  ThrowIfWriteFailed(WriteManifestJson(manifest, manifestPath, &errorMessage), errorMessage, manifestPath);
  engine.cleanup();
}

} // namespace headless_training
