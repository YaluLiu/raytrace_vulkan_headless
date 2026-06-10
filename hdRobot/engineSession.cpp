#include "engineSession.h"

#include "engineSceneSync.h"
#include "sceneStore.h"
#include "camera.h"
#include "glInteropCache.h"
#include "passBridgeConversions.h"
#include "renderParam.h"

#include <engine/engine.hpp>

#include <algorithm>
#include <filesystem>
#include <utility>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

namespace hdrobot {

namespace
{
GfVec2i BootstrapRenderSize()
{
  return GfVec2i(1, 1);
}

bool IsUsdImagingPluginCamera(const CameraData& camera)
{
  constexpr const char* kInternalCameraPrefix = "/_UsdImaging_HdRobotRendererPlugin_";
  constexpr const char* kInternalCameraSuffix = "/camera";
  return camera.name.starts_with(kInternalCameraPrefix) && camera.name.ends_with(kInternalCameraSuffix);
}

void SortCamerasByName(std::vector<CameraData>& cameras)
{
  std::sort(cameras.begin(), cameras.end(), [](const CameraData& lhs, const CameraData& rhs) {
    return lhs.name < rhs.name;
  });
}

void SortLidarSensorsByName(std::vector<LidarSensorData>& sensors)
{
  std::sort(sensors.begin(), sensors.end(), [](const LidarSensorData& lhs,
                                               const LidarSensorData& rhs) {
    return lhs.name < rhs.name;
  });
}

void SortHeightScanSensorsByName(std::vector<HeightScanSensorData>& sensors)
{
  std::sort(sensors.begin(), sensors.end(), [](const HeightScanSensorData& lhs,
                                               const HeightScanSensorData& rhs) {
    return lhs.name < rhs.name;
  });
}
} // namespace

struct EngineSession::Impl
{
  explicit Impl(std::string resourcePath)
      : resourcePath(std::move(resourcePath))
  {
  }

  ~Impl()
  {
    glInteropCache.Clear();
  }

  void EnsureEngineReady(const GfVec2i& renderSize)
  {
    if(!isAppInited)
    {
      InitializeEngine(renderSize);
    }
    else if(currentRenderSize != renderSize)
    {
      ResizeEngine(renderSize);
    }
  }

  void CommitResources(RenderParam& renderParam, SceneStore& sceneStore)
  {
    EnsureEngineReady(isAppInited ? currentRenderSize : BootstrapRenderSize());
    sceneSync.EnsureResources(engine, sceneStore, glInteropCache);

    CacheCommittedFrameState(renderParam, sceneStore);
    SubmitCommittedCameras();
    ConfigureFrameOutputs(requestedTileAovChannels);

    sceneSync.SyncSceneToEngine(engine, sceneStore);
  }

  ::LidarFramePointCloud CaptureLidarPointCloudFrame(RenderParam& renderParam, SceneStore& sceneStore)
  {
    sceneStore.ApplyPendingUpdates();
    CommitResources(renderParam, sceneStore);
    engine.render();
    return engine.readLidarPointCloudFrame();
  }

  void ConfigureFrameOutputs(TileAovChannelMask requestedTileChannels)
  {
    requestedTileAovChannels = requestedTileChannels;
    engine.configureOutputs(ToRendererOutputConfig(tileConfig, requestedTileAovChannels, lidarSensors,
                                                   lidarVisualizationConfig, heightScanSensors,
                                                   heightScanVisualizationConfig));
  }

  bool SetFrameCamera(const HdRenderPassStateSharedPtr& renderPassState)
  {
    const HdCamera* hdCamera = renderPassState ? renderPassState->GetCamera() : nullptr;
    if(!hdCamera)
    {
      return false;
    }

    const HdRobotCamera* robotCamera = dynamic_cast<const HdRobotCamera*>(hdCamera);
    if(robotCamera == nullptr)
    {
      return false;
    }

    CameraData mainCameraData = robotCamera->GetCameraData();
    const CameraConformPolicy frameConformPolicy = ToCameraConformPolicy(renderPassState->GetWindowPolicy());
    mainCameraData.conformPolicy = frameConformPolicy;
    std::vector<CameraData> cameras = camerasSnapshot;
    if(cameras.empty())
    {
      cameras.push_back(mainCameraData);
    }
    else
    {
      const auto cameraIt = std::find_if(cameras.begin(), cameras.end(), [&mainCameraData](const CameraData& camera) {
        return camera.name == mainCameraData.name;
      });
      if(cameraIt == cameras.end())
      {
        cameras.push_back(mainCameraData);
      }
      else
      {
        *cameraIt = mainCameraData;
      }
    }

    cameras.erase(std::remove_if(cameras.begin(), cameras.end(), IsUsdImagingPluginCamera), cameras.end());
    for(CameraData& camera : cameras)
    {
      camera.conformPolicy = frameConformPolicy;
    }
    SortCamerasByName(cameras);

    engine.setCameras(ToCameraSpec(cameras));
    engine.setMainCamera(ToCameraSpec(mainCameraData));
    return true;
  }

  void ApplyFrameRenderTags(const TfTokenVector& renderTags)
  {
    sceneSync.ApplyFrameRenderTags(engine, renderTags);
  }

  void InitializeEngine(const GfVec2i& renderSize)
  {
    if(!resourcePath.empty())
    {
      engine.setPluginSearchRoot(std::filesystem::path(resourcePath).parent_path().string());
    }

    engine.setup(renderSize[0], renderSize[1]);
    currentRenderSize = renderSize;
    isAppInited = true;
  }

  void ResizeEngine(const GfVec2i& renderSize)
  {
    glInteropCache.Clear();
    engine.resize(renderSize[0], renderSize[1]);
    currentRenderSize = renderSize;
  }

  void CacheCommittedFrameState(RenderParam& renderParam, const SceneStore& sceneStore)
  {
    camerasSnapshot = sceneStore.GetActiveCameraSnapshot();
    SortCamerasByName(camerasSnapshot);

    lidarSensors = sceneStore.GetActiveLidarSensorSnapshot();
    SortLidarSensorsByName(lidarSensors);

    heightScanSensors = sceneStore.GetActiveHeightScanSensorSnapshot();
    SortHeightScanSensorsByName(heightScanSensors);

    tileConfig = renderParam.GetTileConfig();
    lidarVisualizationConfig = renderParam.GetLidarVisualizationConfig();
    heightScanVisualizationConfig = renderParam.GetHeightScanVisualizationConfig();
  }

  void SubmitCommittedCameras()
  {
    std::vector<CameraData> cameras = camerasSnapshot;
    cameras.erase(std::remove_if(cameras.begin(), cameras.end(), IsUsdImagingPluginCamera), cameras.end());
    SortCamerasByName(cameras);
    engine.setCameras(ToCameraSpec(cameras));
  }

  std::string resourcePath;
  ::Engine engine;
  GlInteropCache glInteropCache;
  EngineSceneSync sceneSync;
  bool isAppInited = false;
  GfVec2i currentRenderSize{-1, -1};

  std::vector<CameraData> camerasSnapshot;
  std::vector<LidarSensorData> lidarSensors;
  std::vector<HeightScanSensorData> heightScanSensors;
  TileConfig tileConfig;
  LidarVisualizationConfig lidarVisualizationConfig;
  HeightScanVisualizationConfig heightScanVisualizationConfig;
  TileAovChannelMask requestedTileAovChannels = TileAovChannelMask::ColorDepth();
};

EngineSession::EngineSession(std::string resourcePath)
    : _impl(std::make_unique<Impl>(std::move(resourcePath)))
{
}

EngineSession::~EngineSession() = default;

::Engine& EngineSession::GetEngine()
{
  return _impl->engine;
}

const ::Engine& EngineSession::GetEngine() const
{
  return _impl->engine;
}

GlInteropCache& EngineSession::GetGlInteropCache()
{
  return _impl->glInteropCache;
}

void EngineSession::EnsureEngineReady(const GfVec2i& renderSize)
{
  _impl->EnsureEngineReady(renderSize);
}

void EngineSession::CommitResources(RenderParam& renderParam, SceneStore& sceneStore)
{
  _impl->CommitResources(renderParam, sceneStore);
}

::LidarFramePointCloud EngineSession::CaptureLidarPointCloudFrame(RenderParam& renderParam, SceneStore& sceneStore)
{
  return _impl->CaptureLidarPointCloudFrame(renderParam, sceneStore);
}

bool EngineSession::SetFrameCamera(const HdRenderPassStateSharedPtr& renderPassState)
{
  return _impl->SetFrameCamera(renderPassState);
}

void EngineSession::ConfigureFrameOutputs(TileAovChannelMask requestedTileChannels)
{
  _impl->ConfigureFrameOutputs(requestedTileChannels);
}

void EngineSession::ApplyFrameRenderTags(const TfTokenVector& renderTags)
{
  _impl->ApplyFrameRenderTags(renderTags);
}

} // namespace hdrobot

PXR_NAMESPACE_CLOSE_SCOPE
