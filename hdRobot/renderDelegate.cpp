#include "renderDelegate.h"
#include "instancer.h"
#include "renderParam.h"
#include "renderPass.h"

// robot or hdstorm
#include "camera.h"
#include "heightScanSensor.h"
#include "lidarSensor.h"
#include "light.h"
#include "material.h"
#include "mesh.h"
#include "points.h"
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
#include <memory>
#include <unordered_map>

// 娣诲姞hgi
#include "pxr/imaging/hgi/hgi.h"
#include "pxr/imaging/hgi/tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
using RprimFactory = HdRprim *(*)(const SdfPath &, HdRobotRenderParam &);

const static TfTokenVector _supportedRprimTypes = {HdPrimTypeTokens->mesh, HdPrimTypeTokens->points};

const static std::unordered_map<TfToken, RprimFactory, TfToken::HashFunctor> _rprimFactories = {
    {HdPrimTypeTokens->mesh, [](const SdfPath &rprimId, HdRobotRenderParam &renderParam) -> HdRprim *
     { return new HdRobotMesh(rprimId, renderParam); }},
    {HdPrimTypeTokens->points, [](const SdfPath &rprimId, HdRobotRenderParam &renderParam) -> HdRprim *
     { return new HdRobotPoints(rprimId, renderParam); }}};

const static TfTokenVector _supportedSprimTypes = {
    HdPrimTypeTokens->camera, HdPrimTypeTokens->material, HdPrimTypeTokens->sphereLight, HdPrimTypeTokens->distantLight,
    //  HdPrimTypeTokens->rectLight,     HdPrimTypeTokens->diskLight,
    HdPrimTypeTokens->domeLight, HdRaySensorPrimTypeTokens->lidarSensor, HdRaySensorPrimTypeTokens->heightScanSensor,
    HdPrimTypeTokens->simpleLight, // Required for usdview camera light creation
                                   //  HdPrimTypeTokens->extComputation
};

const static TfTokenVector _supportedBprimTypes = {HdPrimTypeTokens->renderBuffer};

HdRenderSettingDescriptorList CreateRenderSettingDescriptors()
{
  const HdRobotTileConfig defaults;
  return {
      HdRenderSettingDescriptor{"Enable tile output", HdRobotRenderSettingTokens->tileEnabled,
                                VtValue(defaults.enabled)},
      HdRenderSettingDescriptor{"Enable tile color output", HdRobotRenderSettingTokens->tileColorEnabled,
                                VtValue(defaults.colorEnabled)},
      HdRenderSettingDescriptor{"Enable tile depth output", HdRobotRenderSettingTokens->tileDepthEnabled,
                                VtValue(defaults.depthEnabled)},
      HdRenderSettingDescriptor{"Tile camera width", HdRobotRenderSettingTokens->tileCameraWidth,
                                VtValue(static_cast<int>(defaults.cameraWidth))},
      HdRenderSettingDescriptor{"Tile camera height", HdRobotRenderSettingTokens->tileCameraHeight,
                                VtValue(static_cast<int>(defaults.cameraHeight))},
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

bool IsColorAov(const TfToken &name)
{
  return name == HdAovTokens->color || name == HdRobotAovTokens->tileColor ||
         name == HdRobotAovTokens->tileDisplayColor;
}

bool IsDepthAov(const TfToken &name)
{
  return name == HdAovTokens->depth || name == HdAovTokens->depthStencil || name == HdRobotAovTokens->tileDepth ||
         name == HdRobotAovTokens->tileDisplayDepth;
}

uint32_t GetPositiveRenderSetting(const HdRenderDelegate &delegate, const TfToken &key, uint32_t fallback)
{
  return static_cast<uint32_t>(std::max(1, delegate.GetRenderSetting<int>(key, static_cast<int>(fallback))));
}

HdRobotTileConfig ReadTileConfig(const HdRenderDelegate &delegate)
{
  HdRobotTileConfig config;
  config.enabled = delegate.GetRenderSetting<bool>(HdRobotRenderSettingTokens->tileEnabled, config.enabled);
  config.colorEnabled =
      delegate.GetRenderSetting<bool>(HdRobotRenderSettingTokens->tileColorEnabled, config.colorEnabled);
  config.depthEnabled =
      delegate.GetRenderSetting<bool>(HdRobotRenderSettingTokens->tileDepthEnabled, config.depthEnabled);
  config.cameraWidth =
      GetPositiveRenderSetting(delegate, HdRobotRenderSettingTokens->tileCameraWidth, config.cameraWidth);
  config.cameraHeight =
      GetPositiveRenderSetting(delegate, HdRobotRenderSettingTokens->tileCameraHeight, config.cameraHeight);
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

HdRobotLidarVisualizationConfig ReadLidarVisualizationConfig(const HdRenderDelegate &delegate)
{
  HdRobotLidarVisualizationConfig config;
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

HdRobotHeightScanVisualizationConfig ReadHeightScanVisualizationConfig(const HdRenderDelegate &delegate)
{
  HdRobotHeightScanVisualizationConfig config;
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
} // namespace

HdRobotRenderDelegate::HdRobotRenderDelegate(const HdRenderSettingsMap &settingsMap, std::string_view resourcePath)
    : HdRenderDelegate(settingsMap), _resourcePath(resourcePath),
      _resourceRegistry(std::make_shared<HdResourceRegistry>()), _renderParam(std::make_unique<HdRobotRenderParam>())
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

const HdCommandDescriptors COMMAND_DESCRIPTORS = {
    HdCommandDescriptor{HdRobotCommandTokens->printLicenses, "Print Licenses"}};

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

  TF_RUNTIME_ERROR("Unsupported command %s", command.GetText());

  return false;
}

HdRenderPassSharedPtr HdRobotRenderDelegate::CreateRenderPass(HdRenderIndex *index, const HdRprimCollection &collection)
{
  HdRobotRenderParam *robotRenderParam = _GetRobotRenderParam();
  if(!TF_VERIFY(robotRenderParam != nullptr))
  {
    return nullptr;
  }

  return HdRenderPassSharedPtr(new HdRobotRenderPass(index, collection, *robotRenderParam, _resourcePath));
}

HdResourceRegistrySharedPtr HdRobotRenderDelegate::GetResourceRegistry() const
{
  return _resourceRegistry;
}

void HdRobotRenderDelegate::CommitResources(HdChangeTracker *tracker)
{
  TF_UNUSED(tracker);
  // We delay BVH building and GPU uploads to the next render call.
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
  if(IsColorAov(name))
  {
    return HdAovDescriptor(HdFormatFloat32Vec4, true, VtValue(GfVec4f(1.0f)));
  }
  else if(IsDepthAov(name))
  {
    return HdAovDescriptor(HdFormatFloat32, false, VtValue(1.0f));
  }
  else if(name == HdAovTokens->primId || name == HdAovTokens->instanceId)
  {
    return HdAovDescriptor(HdFormatInt32, false, VtValue(-1));
  }

  return HdAovDescriptor();
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
    HdRobotRenderParam *robotRenderParam = _GetRobotRenderParam();
    if(!TF_VERIFY(robotRenderParam != nullptr))
    {
      return nullptr;
    }
    return factoryIt->second(rprimId, *robotRenderParam);
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
  HdRobotRenderParam *robotRenderParam = _GetRobotRenderParam();
  if(!TF_VERIFY(robotRenderParam != nullptr))
  {
    return nullptr;
  }

  if(typeId == HdPrimTypeTokens->camera)
  {
    return new HdRobotCamera(sprimId, *robotRenderParam);
  }
  else if(typeId == HdRaySensorPrimTypeTokens->lidarSensor)
  {
    return new HdRobotLidarSensor(sprimId, *robotRenderParam);
  }
  else if(typeId == HdRaySensorPrimTypeTokens->heightScanSensor)
  {
    return new HdRobotHeightScanSensor(sprimId, *robotRenderParam);
  }
  else if(typeId == HdPrimTypeTokens->material)
  {
    return new HdRobotMaterial(sprimId, *robotRenderParam);
  }
  else if(typeId == HdPrimTypeTokens->distantLight)
  {
    return new HdRobotDistantLight(sprimId, *robotRenderParam);
  }
  else if(typeId == HdPrimTypeTokens->sphereLight)
  {
    return new HdRobotSphereLight(sprimId, *robotRenderParam);
  }
  else if(typeId == HdPrimTypeTokens->simpleLight)
  {
    return new HdRobotSimpleLight(sprimId, *robotRenderParam);
  }
  else if(typeId == HdPrimTypeTokens->domeLight)
  {
    return new HdRobotDomeLight(sprimId, *robotRenderParam);
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

HdRobotRenderParam *HdRobotRenderDelegate::_GetRobotRenderParam() const
{
  return _renderParam.get();
}

PXR_NAMESPACE_CLOSE_SCOPE
