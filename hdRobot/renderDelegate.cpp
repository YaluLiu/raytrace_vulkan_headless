#include "renderDelegate.h"
#include "instancer.h"
#include "renderParam.h"
#include "renderPass.h"
#include "engineSession.h"

// robot or hdstorm
#include "aovBridgeSpec.h"
#include "camera.h"
#include "heightScanSensor.h"
#include "lidarSensor.h"
#include "light.h"
#include "material.h"
#include "mesh.h"
#include "renderBuffer.h"
// #include "pxr/imaging/hdSt/renderBuffer.h"
#include "tokens.h"
#include "../UsdRaySensorImaging/hydraSensor.h"

#include <pxr/base/arch/fileSystem.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/hd/extComputation.h>
#include <pxr/imaging/hd/resourceRegistry.h>

// 娣诲姞hdlight
#include "pxr/imaging/hdSt/light.h"
#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <unordered_map>

// 娣诲姞hgi
#include "pxr/imaging/hgi/hgi.h"
#include "pxr/imaging/hgi/tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

using hdrobot::EngineSession;
using hdrobot::CameraHandle;
using hdrobot::HeightScanSensorHandle;
using hdrobot::HeightScanVisualizationConfig;
using hdrobot::LidarSensorHandle;
using hdrobot::LidarVisualizationConfig;
using hdrobot::LightHandle;
using hdrobot::MaterialHandle;
using hdrobot::MeshHandle;
using hdrobot::RenderParam;
using hdrobot::SceneStore;
using hdrobot::TileConfig;

namespace
{
using RprimFactory = HdRprim *(*)(const SdfPath &, SceneStore &);

const static TfTokenVector _supportedRprimTypes = {HdPrimTypeTokens->mesh};

const static std::unordered_map<TfToken, RprimFactory, TfToken::HashFunctor> _rprimFactories = {
    {HdPrimTypeTokens->mesh, [](const SdfPath &rprimId, SceneStore &sceneStore) -> HdRprim *
     {
       const MeshHandle meshHandle = sceneStore.CreateMesh(rprimId);
       const MaterialHandle displayColorMaterialHandle =
           sceneStore.CreateMaterial(rprimId.AppendChild(TfToken("__displayColorMaterial")));
       return new HdRobotMesh(rprimId, sceneStore, meshHandle, displayColorMaterialHandle);
     }}};

const static TfTokenVector _supportedSprimTypes = {
    HdPrimTypeTokens->camera, HdPrimTypeTokens->material, HdPrimTypeTokens->sphereLight, HdPrimTypeTokens->distantLight,
    //  HdPrimTypeTokens->rectLight,     HdPrimTypeTokens->diskLight,
    HdPrimTypeTokens->domeLight, HdRaySensorPrimTypeTokens->lidarSensor, HdRaySensorPrimTypeTokens->heightScanSensor,
    HdPrimTypeTokens->simpleLight, // Required for usdview camera light creation
                                   //  HdPrimTypeTokens->extComputation
};

const static TfTokenVector _supportedBprimTypes = {HdPrimTypeTokens->renderBuffer};

constexpr const char *kDefaultLidarPointCloudPath = "lidar_point_cloud.csv";

HdRenderSettingDescriptorList CreateRenderSettingDescriptors()
{
  const TileConfig defaults;
  return {
      HdRenderSettingDescriptor{"Enable tile output", HdRobotRenderSettingTokens->tileEnabled,
                                VtValue(defaults.enabled)},
      HdRenderSettingDescriptor{"Enable tile color output", HdRobotRenderSettingTokens->tileColorEnabled,
                                VtValue(defaults.colorEnabled)},
      HdRenderSettingDescriptor{"Enable tile depth output", HdRobotRenderSettingTokens->tileDepthEnabled,
                                VtValue(defaults.depthEnabled)},
      HdRenderSettingDescriptor{"Tile camera width", HdRobotRenderSettingTokens->tileCameraWidth,
                                VtValue(static_cast<int>(defaults.tilePixelWidth))},
      HdRenderSettingDescriptor{"Tile camera height", HdRobotRenderSettingTokens->tileCameraHeight,
                                VtValue(static_cast<int>(defaults.tilePixelHeight))},
      HdRenderSettingDescriptor{"Tile grid columns", HdRobotRenderSettingTokens->tileGridColumns,
                                VtValue(static_cast<int>(defaults.gridColumns))},
      HdRenderSettingDescriptor{"Tile grid rows", HdRobotRenderSettingTokens->tileGridRows,
                                VtValue(static_cast<int>(defaults.gridRows))},
      HdRenderSettingDescriptor{"Enable LiDAR point cloud visualization",
                                HdRobotRenderSettingTokens->lidarVisualizeEnabled, VtValue(false)},
      HdRenderSettingDescriptor{"Visualize all LiDAR point clouds",
                                HdRobotRenderSettingTokens->lidarVisualizeAllSensors, VtValue(false)},
      HdRenderSettingDescriptor{"LiDAR visualization sensor index",
                                HdRobotRenderSettingTokens->lidarVisualizeSensorIndex, VtValue(0)},
      HdRenderSettingDescriptor{"LiDAR visualization point size",
                                HdRobotRenderSettingTokens->lidarVisualizePointSize, VtValue(2.0f)},
      HdRenderSettingDescriptor{"Enable height scan point cloud visualization",
                                HdRobotRenderSettingTokens->heightScanVisualizeEnabled, VtValue(false)},
      HdRenderSettingDescriptor{"Visualize all height scan point clouds",
                                HdRobotRenderSettingTokens->heightScanVisualizeAllSensors, VtValue(false)},
      HdRenderSettingDescriptor{"Height scan visualization sensor index",
                                HdRobotRenderSettingTokens->heightScanVisualizeSensorIndex, VtValue(0)},
      HdRenderSettingDescriptor{"Height scan visualization point size",
                                HdRobotRenderSettingTokens->heightScanVisualizePointSize, VtValue(2.0f)},
  };
}

bool IsTileRenderSetting(const TfToken &key)
{
  return key == HdRobotRenderSettingTokens->tileEnabled || key == HdRobotRenderSettingTokens->tileColorEnabled ||
         key == HdRobotRenderSettingTokens->tileDepthEnabled || key == HdRobotRenderSettingTokens->tileCameraWidth ||
         key == HdRobotRenderSettingTokens->tileCameraHeight || key == HdRobotRenderSettingTokens->tileGridColumns ||
         key == HdRobotRenderSettingTokens->tileGridRows;
}

bool IsLidarRenderSetting(const TfToken &key)
{
  return key == HdRobotRenderSettingTokens->lidarVisualizeEnabled ||
         key == HdRobotRenderSettingTokens->lidarVisualizeAllSensors ||
         key == HdRobotRenderSettingTokens->lidarVisualizeSensorIndex ||
         key == HdRobotRenderSettingTokens->lidarVisualizePointSize;
}

bool IsHeightScanRenderSetting(const TfToken &key)
{
  return key == HdRobotRenderSettingTokens->heightScanVisualizeEnabled ||
         key == HdRobotRenderSettingTokens->heightScanVisualizeAllSensors ||
         key == HdRobotRenderSettingTokens->heightScanVisualizeSensorIndex ||
         key == HdRobotRenderSettingTokens->heightScanVisualizePointSize;
}

uint32_t GetPositiveRenderSetting(const HdRenderDelegate &delegate, const TfToken &key, uint32_t fallback)
{
  return static_cast<uint32_t>(std::max(1, delegate.GetRenderSetting<int>(key, static_cast<int>(fallback))));
}

TileConfig ReadTileConfig(const HdRenderDelegate &delegate)
{
  TileConfig config;
  config.enabled = delegate.GetRenderSetting<bool>(HdRobotRenderSettingTokens->tileEnabled, config.enabled);
  config.colorEnabled =
      delegate.GetRenderSetting<bool>(HdRobotRenderSettingTokens->tileColorEnabled, config.colorEnabled);
  config.depthEnabled =
      delegate.GetRenderSetting<bool>(HdRobotRenderSettingTokens->tileDepthEnabled, config.depthEnabled);
  config.tilePixelWidth =
      GetPositiveRenderSetting(delegate, HdRobotRenderSettingTokens->tileCameraWidth, config.tilePixelWidth);
  config.tilePixelHeight =
      GetPositiveRenderSetting(delegate, HdRobotRenderSettingTokens->tileCameraHeight, config.tilePixelHeight);
  config.gridColumns =
      GetPositiveRenderSetting(delegate, HdRobotRenderSettingTokens->tileGridColumns, config.gridColumns);
  config.gridRows = GetPositiveRenderSetting(delegate, HdRobotRenderSettingTokens->tileGridRows, config.gridRows);
  return config;
}

float GetPositiveFloatRenderSetting(const HdRenderDelegate &delegate, const TfToken &key, float fallback)
{
  const float value = delegate.GetRenderSetting<float>(key, fallback);
  return std::isfinite(value) ? std::max(value, 0.0f) : fallback;
}

LidarVisualizationConfig ReadLidarVisualizationConfig(const HdRenderDelegate &delegate)
{
  LidarVisualizationConfig config;
  config.enabled =
      delegate.GetRenderSetting<bool>(HdRobotRenderSettingTokens->lidarVisualizeEnabled, config.enabled);
  config.visualizeAllSensors = delegate.GetRenderSetting<bool>(
      HdRobotRenderSettingTokens->lidarVisualizeAllSensors, config.visualizeAllSensors);
  config.sensorIndex = static_cast<uint32_t>(
      std::max(0, delegate.GetRenderSetting<int>(HdRobotRenderSettingTokens->lidarVisualizeSensorIndex,
                                                 static_cast<int>(config.sensorIndex))));
  config.pointSizePixels = GetPositiveFloatRenderSetting(
      delegate, HdRobotRenderSettingTokens->lidarVisualizePointSize, config.pointSizePixels);
  return config;
}

HeightScanVisualizationConfig ReadHeightScanVisualizationConfig(const HdRenderDelegate &delegate)
{
  HeightScanVisualizationConfig config;
  config.enabled =
      delegate.GetRenderSetting<bool>(HdRobotRenderSettingTokens->heightScanVisualizeEnabled, config.enabled);
  config.visualizeAllSensors = delegate.GetRenderSetting<bool>(
      HdRobotRenderSettingTokens->heightScanVisualizeAllSensors, config.visualizeAllSensors);
  config.sensorIndex = static_cast<uint32_t>(
      std::max(0, delegate.GetRenderSetting<int>(HdRobotRenderSettingTokens->heightScanVisualizeSensorIndex,
                                                 static_cast<int>(config.sensorIndex))));
  config.pointSizePixels = GetPositiveFloatRenderSetting(
      delegate, HdRobotRenderSettingTokens->heightScanVisualizePointSize, config.pointSizePixels);
  return config;
}

std::string GetCommandStringArg(const HdCommandArgs &args, const TfToken &key, const std::string &fallback)
{
  const HdCommandArgs::const_iterator argIt = args.find(key.GetString());
  if(argIt == args.end())
  {
    return fallback;
  }

  const VtValue &value = argIt->second;
  if(value.IsHolding<std::string>())
  {
    const std::string &result = value.UncheckedGet<std::string>();
    return result.empty() ? fallback : result;
  }
  if(value.IsHolding<TfToken>())
  {
    const std::string result = value.UncheckedGet<TfToken>().GetString();
    return result.empty() ? fallback : result;
  }

  TF_WARN("Ignoring command argument %s: expected string, got %s.",
          key.GetText(),
          value.GetTypeName().c_str());
  return fallback;
}

bool HasLidarPointFlag(uint32_t flags, uint32_t flag)
{
  return (flags & flag) != 0;
}

void WriteCsvString(std::ostream &output, const std::string &value)
{
  if(value.find_first_of(",\"\n\r") == std::string::npos)
  {
    output << value;
    return;
  }

  output << '"';
  for(const char c : value)
  {
    if(c == '"')
    {
      output << "\"\"";
    }
    else
    {
      output << c;
    }
  }
  output << '"';
}

size_t CountLidarPoints(const LidarFramePointCloud &frame)
{
  size_t result = 0;
  for(const LidarSensorPointCloud &sensor : frame.sensors)
  {
    result += sensor.points.size();
  }
  return result;
}

bool WriteLidarPointCloudCsv(const LidarFramePointCloud &frame,
                             const std::string &filePath,
                             std::string *errorMessage)
{
  try
  {
    const std::filesystem::path outputPath(filePath);
    if(outputPath.has_parent_path())
    {
      std::filesystem::create_directories(outputPath.parent_path());
    }

    std::ofstream output(outputPath, std::ios::out | std::ios::trunc);
    if(!output)
    {
      if(errorMessage != nullptr)
      {
        *errorMessage = "failed to open output file";
      }
      return false;
    }

    output << std::setprecision(9);
    output << "frame_id,sensor_name,sensor_index,width,height,point_index,ring_index,beam_index,"
              "x,y,z,range_meters,intensity,flags,valid,hit,out_of_range\n";

    for(const LidarSensorPointCloud &sensor : frame.sensors)
    {
      for(size_t pointIndex = 0; pointIndex < sensor.points.size(); ++pointIndex)
      {
        const LidarPoint &point = sensor.points[pointIndex];
        output << frame.frameId << ',';
        WriteCsvString(output, sensor.name);
        output << ',' << sensor.sensorIndex << ',' << sensor.width << ',' << sensor.height << ',' << pointIndex
               << ',' << point.ringIndex << ',' << point.beamIndex << ',' << point.positionWs.x << ','
               << point.positionWs.y << ',' << point.positionWs.z << ',' << point.rangeMeters << ','
               << point.intensity << ',' << point.flags << ','
               << (HasLidarPointFlag(point.flags, LidarPointFlagValid) ? 1 : 0) << ','
               << (HasLidarPointFlag(point.flags, LidarPointFlagHit) ? 1 : 0) << ','
               << (HasLidarPointFlag(point.flags, LidarPointFlagOutOfRange) ? 1 : 0) << '\n';
      }
    }

    if(!output)
    {
      if(errorMessage != nullptr)
      {
        *errorMessage = "failed while writing output file";
      }
      return false;
    }
  }
  catch(const std::exception &e)
  {
    if(errorMessage != nullptr)
    {
      *errorMessage = e.what();
    }
    return false;
  }

  return true;
}
} // namespace

HdRobotRenderDelegate::HdRobotRenderDelegate(const HdRenderSettingsMap &settingsMap, std::string_view resourcePath)
    : HdRenderDelegate(settingsMap), _resourcePath(resourcePath),
      _resourceRegistry(std::make_shared<HdResourceRegistry>()), _renderParam(std::make_unique<RenderParam>()),
      _engineSession(std::make_unique<EngineSession>(_resourcePath))
{
  _settingDescriptors = CreateRenderSettingDescriptors();
  _PopulateDefaultSettings(_settingDescriptors);
  _SyncTileConfigFromSettings();
  _SyncLidarVisualizationConfigFromSettings();
  _SyncHeightScanVisualizationConfigFromSettings();
}

HdRobotRenderDelegate::~HdRobotRenderDelegate() {}

HdRenderSettingDescriptorList HdRobotRenderDelegate::GetRenderSettingDescriptors() const
{
  return _settingDescriptors;
}

void HdRobotRenderDelegate::SetRenderSetting(const TfToken &key, const VtValue &value)
{
  HdRenderDelegate::SetRenderSetting(key, value);
  if(IsTileRenderSetting(key))
  {
    _SyncTileConfigFromSettings();
  }
  if(IsLidarRenderSetting(key))
  {
    _SyncLidarVisualizationConfigFromSettings();
  }
  if(IsHeightScanRenderSetting(key))
  {
    _SyncHeightScanVisualizationConfigFromSettings();
  }
}

static HdCommandDescriptors CreateCommandDescriptors()
{
  return {
      HdCommandDescriptor{HdRobotCommandTokens->printLicenses, "Print Licenses"},
      HdCommandDescriptor{
          HdRobotCommandTokens->exportLidarPointCloud,
          "Export LiDAR Point Cloud",
          HdCommandArgDescriptors{
              HdCommandArgDescriptor{HdRobotCommandArgTokens->filePath,
                                     VtValue(std::string(kDefaultLidarPointCloudPath))}}},
  };
}

const HdCommandDescriptors COMMAND_DESCRIPTORS = CreateCommandDescriptors();

HdCommandDescriptors HdRobotRenderDelegate::GetCommandDescriptors() const
{
  return COMMAND_DESCRIPTORS;
}

bool HdRobotRenderDelegate::InvokeCommand(const TfToken &command, [[maybe_unused]] const HdCommandArgs &args)
{
  if(command == HdRobotCommandTokens->printLicenses)
  {
    std::string licenseFilePath = TfStringCatPaths(_resourcePath, "LICENSE");
    std::string errorMessage;

    ArchConstFileMapping mapping = ArchMapFileReadOnly(licenseFilePath, &errorMessage);
    if(!mapping)
    {
      TF_RUNTIME_ERROR("Can't execute command: %s", errorMessage.c_str());
      return false;
    }

    const char *licenseText = mapping.get();

    printf("%s\n", licenseText);
    fflush(stdout);

    return true;
  }
  if(command == HdRobotCommandTokens->exportLidarPointCloud)
  {
    if(_renderParam == nullptr || _engineSession == nullptr)
    {
      TF_RUNTIME_ERROR("Can't execute command: render delegate resources are not initialized");
      return false;
    }

    const std::string filePath =
        GetCommandStringArg(args, HdRobotCommandArgTokens->filePath, kDefaultLidarPointCloudPath);
    const LidarFramePointCloud frame =
        _engineSession->CaptureLidarPointCloudFrame(*_renderParam, _sceneStore);

    std::string errorMessage;
    if(!WriteLidarPointCloudCsv(frame, filePath, &errorMessage))
    {
      TF_RUNTIME_ERROR("Can't export LiDAR point cloud to %s: %s",
                       filePath.c_str(),
                       errorMessage.c_str());
      return false;
    }

    printf("Exported LiDAR point cloud frame %llu (%zu sensors, %zu points) to %s\n",
           static_cast<unsigned long long>(frame.frameId),
           frame.sensors.size(),
           CountLidarPoints(frame),
           filePath.c_str());
    fflush(stdout);

    return true;
  }

  TF_RUNTIME_ERROR("Unsupported command %s", command.GetText());

  return false;
}

HdRenderPassSharedPtr HdRobotRenderDelegate::CreateRenderPass(HdRenderIndex *index, const HdRprimCollection &collection)
{
  if(!TF_VERIFY(_engineSession != nullptr))
  {
    return nullptr;
  }

  return HdRenderPassSharedPtr(new HdRobotRenderPass(index, collection, *_engineSession));
}

HdResourceRegistrySharedPtr HdRobotRenderDelegate::GetResourceRegistry() const
{
  return _resourceRegistry;
}

void HdRobotRenderDelegate::CommitResources(HdChangeTracker *tracker)
{
  TF_UNUSED(tracker);
  if(_renderParam)
  {
    _sceneStore.ApplyPendingUpdates();
    if(_engineSession)
    {
      _engineSession->CommitResources(*_renderParam, _sceneStore);
    }
  }
}

HdInstancer *HdRobotRenderDelegate::CreateInstancer(HdSceneDelegate *delegate, const SdfPath &id)
{
  return new HdRobotInstancer(delegate, id);
}

void HdRobotRenderDelegate::DestroyInstancer(HdInstancer *instancer)
{
  delete instancer;
}

HdAovDescriptor HdRobotRenderDelegate::GetDefaultAovDescriptor(const TfToken &name) const
{
  return GetHdRobotDefaultAovDescriptor(name);
}

HdRenderParam *HdRobotRenderDelegate::GetRenderParam() const
{
  return _renderParam.get();
}

void HdRobotRenderDelegate::_SyncTileConfigFromSettings()
{
  if(_renderParam)
  {
    _renderParam->SetTileConfig(ReadTileConfig(*this));
  }
}

void HdRobotRenderDelegate::_SyncLidarVisualizationConfigFromSettings()
{
  if(_renderParam)
  {
    _renderParam->SetLidarVisualizationConfig(ReadLidarVisualizationConfig(*this));
  }
}

void HdRobotRenderDelegate::_SyncHeightScanVisualizationConfigFromSettings()
{
  if(_renderParam)
  {
    _renderParam->SetHeightScanVisualizationConfig(ReadHeightScanVisualizationConfig(*this));
  }
}

const TfTokenVector &HdRobotRenderDelegate::GetSupportedRprimTypes() const
{
  return _supportedRprimTypes;
}

HdRprim *HdRobotRenderDelegate::CreateRprim(const TfToken &typeId, const SdfPath &rprimId)
{
  const auto factoryIt = _rprimFactories.find(typeId);
  if(factoryIt != _rprimFactories.end())
  {
    return factoryIt->second(rprimId, _sceneStore);
  }

  return nullptr;
}

void HdRobotRenderDelegate::DestroyRprim(HdRprim *rprim)
{
  delete rprim;
}

const TfTokenVector &HdRobotRenderDelegate::GetSupportedSprimTypes() const
{
  return _supportedSprimTypes;
}

HdSprim *HdRobotRenderDelegate::CreateSprim(const TfToken &typeId, const SdfPath &sprimId)
{
  if(typeId == HdPrimTypeTokens->camera)
  {
    const CameraHandle handle =
        sprimId.IsEmpty() ? CameraHandle{} : _sceneStore.CreateCamera(sprimId);
    return new HdRobotCamera(sprimId, _sceneStore, handle);
  }
  else if(typeId == HdRaySensorPrimTypeTokens->lidarSensor)
  {
    const LidarSensorHandle handle =
        sprimId.IsEmpty() ? LidarSensorHandle{} : _sceneStore.CreateLidarSensor(sprimId);
    return new HdRobotLidarSensor(sprimId, _sceneStore, handle);
  }
  else if(typeId == HdRaySensorPrimTypeTokens->heightScanSensor)
  {
    const HeightScanSensorHandle handle = sprimId.IsEmpty()
                                                     ? HeightScanSensorHandle{}
                                                     : _sceneStore.CreateHeightScanSensor(sprimId);
    return new HdRobotHeightScanSensor(sprimId, _sceneStore, handle);
  }
  else if(typeId == HdPrimTypeTokens->material)
  {
    const MaterialHandle handle =
        sprimId.IsEmpty() ? MaterialHandle{} : _sceneStore.CreateMaterial(sprimId);
    return new HdRobotMaterial(sprimId, _sceneStore, handle);
  }
  else if(typeId == HdPrimTypeTokens->distantLight)
  {
    const LightHandle handle =
        sprimId.IsEmpty() ? LightHandle{} : _sceneStore.CreateLight(sprimId);
    return new HdRobotDistantLight(sprimId, _sceneStore, handle);
  }
  else if(typeId == HdPrimTypeTokens->sphereLight)
  {
    const LightHandle handle =
        sprimId.IsEmpty() ? LightHandle{} : _sceneStore.CreateLight(sprimId);
    return new HdRobotSphereLight(sprimId, _sceneStore, handle);
  }
  else if(typeId == HdPrimTypeTokens->simpleLight)
  {
    const LightHandle handle =
        sprimId.IsEmpty() ? LightHandle{} : _sceneStore.CreateLight(sprimId);
    return new HdRobotSimpleLight(sprimId, _sceneStore, handle);
  }
  else if(typeId == HdPrimTypeTokens->domeLight)
  {
    const LightHandle handle =
        sprimId.IsEmpty() ? LightHandle{} : _sceneStore.CreateLight(sprimId);
    return new HdRobotDomeLight(sprimId, _sceneStore, handle);
  }
  return nullptr;
}

HdSprim *HdRobotRenderDelegate::CreateFallbackSprim(const TfToken &typeId)
{
  const SdfPath &sprimId = SdfPath::EmptyPath();

  return CreateSprim(typeId, sprimId);
}

void HdRobotRenderDelegate::DestroySprim(HdSprim *sprim)
{
  delete sprim;
}

const TfTokenVector &HdRobotRenderDelegate::GetSupportedBprimTypes() const
{
  return _supportedBprimTypes;
}

HdBprim *HdRobotRenderDelegate::CreateBprim(const TfToken &typeId, const SdfPath &bprimId)
{
  if(typeId == HdPrimTypeTokens->renderBuffer)
  {
    return new HdRobotRenderBuffer(bprimId, this);
  }

  return nullptr;
}

HdBprim *HdRobotRenderDelegate::CreateFallbackBprim(const TfToken &typeId)
{
  const SdfPath &bprimId = SdfPath::EmptyPath();

  return CreateBprim(typeId, bprimId);
}

void HdRobotRenderDelegate::DestroyBprim(HdBprim *bprim)
{
  delete bprim;
}

TfToken HdRobotRenderDelegate::GetMaterialBindingPurpose() const
{
  return HdTokens->full;
}

TfTokenVector HdRobotRenderDelegate::GetMaterialRenderContexts() const
{
  return TfTokenVector{HdRobotRenderContexts->mtlx, HdRobotRenderContexts->mdl};
}

TfTokenVector HdRobotRenderDelegate::GetShaderSourceTypes() const
{
  return TfTokenVector{HdRobotSourceTypes->mtlx, HdRobotSourceTypes->mdl};
}

#if PXR_VERSION >= 2408
bool HdRobotRenderDelegate::IsParallelSyncEnabled(const TfToken &primType) const
{
  return primType == HdPrimTypeTokens->mesh || primType == HdPrimTypeTokens->material ||
         primType == HdPrimTypeTokens->instancer;
}
#endif

Hgi *HdRobotRenderDelegate::GetHgi()
{
  return _hgi;
}

void HdRobotRenderDelegate::SetDrivers(HdDriverVector const &drivers)
{
  // For Storm we want to use the Hgi driver, so extract it.
  for(HdDriver *hdDriver : drivers)
  {
    if(hdDriver->name == HgiTokens->renderDriver && hdDriver->driver.IsHolding<Hgi *>())
    {
      _hgi = hdDriver->driver.UncheckedGet<Hgi *>();
      break;
    }
  }

  TF_VERIFY(_hgi, "HdSt requires Hgi HdDriver");
}

PXR_NAMESPACE_CLOSE_SCOPE
