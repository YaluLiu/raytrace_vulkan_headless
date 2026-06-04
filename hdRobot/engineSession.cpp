#include "engineSession.h"

#include "engineSceneSync.h"
#include "sceneStore.h"
#include "camera.h"
#include "glInteropCache.h"
#include "renderBridgeConversions.h"
#include "renderParam.h"

#include <engine/engine.hpp>

#include <algorithm>
#include <filesystem>
#include <utility>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
GfVec2i BootstrapRenderSize()
{
  return GfVec2i(1, 1);
}

bool IsUsdImagingPluginCamera(const HdRobotCameraData& camera)
{
  constexpr const char* kInternalCameraPrefix = "/_UsdImaging_HdRobotRendererPlugin_";
  constexpr const char* kInternalCameraSuffix = "/camera";
  return camera.name.starts_with(kInternalCameraPrefix) && camera.name.ends_with(kInternalCameraSuffix);
}

void SortCamerasByName(std::vector<HdRobotCameraData>& cameras)
{
  std::sort(cameras.begin(), cameras.end(), [](const HdRobotCameraData& lhs, const HdRobotCameraData& rhs) {
    return lhs.name < rhs.name;
  });
}

void SortLidarSensorsByName(std::vector<HdRobotLidarSensorData>& sensors)
{
  std::sort(sensors.begin(), sensors.end(), [](const HdRobotLidarSensorData& lhs,
                                               const HdRobotLidarSensorData& rhs) {
    return lhs.name < rhs.name;
  });
}

void SortHeightScanSensorsByName(std::vector<HdRobotHeightScanSensorData>& sensors)
{
  std::sort(sensors.begin(), sensors.end(), [](const HdRobotHeightScanSensorData& lhs,
                                               const HdRobotHeightScanSensorData& rhs) {
    return lhs.name < rhs.name;
  });
}
} // namespace

struct HdRobotEngineSession::Impl
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

  void CommitResources(HdRobotRenderParam& renderParam, HdRobotSceneStore& sceneStore)
  {
    EnsureEngineReady(isAppInited ? currentRenderSize : BootstrapRenderSize());
    sceneSync.EnsureResources(engine, sceneStore, glInteropCache);

    CacheCommittedFrameState(renderParam, sceneStore);
    SubmitCommittedCameras();
    ConfigureFrameOutputs(requestedTileAovChannels);

    sceneSync.SyncSceneToEngine(engine, sceneStore);
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

    HdRobotCameraData mainCameraData = robotCamera->GetCameraData();
    const CameraConformPolicy frameConformPolicy = ToCameraConformPolicy(renderPassState->GetWindowPolicy());
    mainCameraData.conformPolicy = frameConformPolicy;
    std::vector<HdRobotCameraData> cameras = camerasSnapshot;
    if(cameras.empty())
    {
      cameras.push_back(mainCameraData);
    }
    else
    {
      const auto cameraIt = std::find_if(cameras.begin(), cameras.end(), [&mainCameraData](const HdRobotCameraData& camera) {
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
    for(HdRobotCameraData& camera : cameras)
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

  void CacheCommittedFrameState(HdRobotRenderParam& renderParam, const HdRobotSceneStore& sceneStore)
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
    std::vector<HdRobotCameraData> cameras = camerasSnapshot;
    cameras.erase(std::remove_if(cameras.begin(), cameras.end(), IsUsdImagingPluginCamera), cameras.end());
    SortCamerasByName(cameras);
    engine.setCameras(ToCameraSpec(cameras));
  }

  std::string resourcePath;
  ::Engine engine;
  ::HdRobotGlInteropCache glInteropCache;
  HdRobotEngineSceneSync sceneSync;
  bool isAppInited = false;
  GfVec2i currentRenderSize{-1, -1};

  std::vector<HdRobotCameraData> camerasSnapshot;
  std::vector<HdRobotLidarSensorData> lidarSensors;
  std::vector<HdRobotHeightScanSensorData> heightScanSensors;
  HdRobotTileConfig tileConfig;
  HdRobotLidarVisualizationConfig lidarVisualizationConfig;
  HdRobotHeightScanVisualizationConfig heightScanVisualizationConfig;
  TileAovChannelMask requestedTileAovChannels = TileAovChannelMask::ColorDepth();
};

HdRobotEngineSession::HdRobotEngineSession(std::string resourcePath)
    : _impl(std::make_unique<Impl>(std::move(resourcePath)))
{
}

HdRobotEngineSession::~HdRobotEngineSession() = default;

::Engine& HdRobotEngineSession::GetEngine()
{
  return _impl->engine;
}

const ::Engine& HdRobotEngineSession::GetEngine() const
{
  return _impl->engine;
}

::HdRobotGlInteropCache& HdRobotEngineSession::GetGlInteropCache()
{
  return _impl->glInteropCache;
}

void HdRobotEngineSession::EnsureEngineReady(const GfVec2i& renderSize)
{
  _impl->EnsureEngineReady(renderSize);
}

void HdRobotEngineSession::CommitResources(HdRobotRenderParam& renderParam, HdRobotSceneStore& sceneStore)
{
  _impl->CommitResources(renderParam, sceneStore);
}

bool HdRobotEngineSession::SetFrameCamera(const HdRenderPassStateSharedPtr& renderPassState)
{
  return _impl->SetFrameCamera(renderPassState);
}

void HdRobotEngineSession::ConfigureFrameOutputs(TileAovChannelMask requestedTileChannels)
{
  _impl->ConfigureFrameOutputs(requestedTileChannels);
}

void HdRobotEngineSession::ApplyFrameRenderTags(const TfTokenVector& renderTags)
{
  _impl->ApplyFrameRenderTags(renderTags);
}

PXR_NAMESPACE_CLOSE_SCOPE
