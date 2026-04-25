//
// Copyright (C) 2019-2022 Pablo Delgado Kr盲mer
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.
//

#include "renderDelegate.h"
#include "renderParam.h"
#include "renderPass.h"
#include "instancer.h"

//robot or hdstorm
#include "mesh.h"
#include "material.h"
#include "light.h"
#include "camera.h"
#include "points.h"
#include "renderBuffer.h"
//#include "pxr/imaging/hdSt/renderBuffer.h"
#include "tokens.h"

#include <pxr/base/arch/fileSystem.h>
#include <pxr/imaging/hd/extComputation.h>
#include <pxr/imaging/hd/resourceRegistry.h>
#include <pxr/imaging/hd/camera.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec4f.h>


//娣诲姞hdlight
#include "pxr/imaging/hdSt/light.h"
#include <memory>
#include <unordered_map>

//娣诲姞hgi
#include "pxr/imaging/hgi/hgi.h"
#include "pxr/imaging/hgi/tokens.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace {
using RprimFactory = HdRprim* (*)(const SdfPath&, HdRobotRenderParam&);

const static TfTokenVector _supportedRprimTypes = {HdPrimTypeTokens->mesh, HdPrimTypeTokens->points};

const static std::unordered_map<TfToken, RprimFactory, TfToken::HashFunctor> _rprimFactories = {
    {HdPrimTypeTokens->mesh,
     [](const SdfPath& rprimId, HdRobotRenderParam& renderParam) -> HdRprim* {
       return new HdRobotMesh(rprimId, renderParam);
     }},
    {HdPrimTypeTokens->points,
     [](const SdfPath& rprimId, HdRobotRenderParam& renderParam) -> HdRprim* {
       return new HdRobotPoints(rprimId, renderParam);
     }}};

const static TfTokenVector _supportedSprimTypes = {
    HdPrimTypeTokens->camera,
    HdPrimTypeTokens->material,
    HdPrimTypeTokens->sphereLight,
    HdPrimTypeTokens->distantLight,
    //  HdPrimTypeTokens->rectLight,     HdPrimTypeTokens->diskLight,
    HdPrimTypeTokens->domeLight,
    //  HdPrimTypeTokens->simpleLight,  // Required for usdview domeLight creation
    //  HdPrimTypeTokens->extComputation
};

const static TfTokenVector _supportedBprimTypes = {HdPrimTypeTokens->renderBuffer};
}  // namespace

HdRobotRenderDelegate::HdRobotRenderDelegate(const HdRenderSettingsMap& settingsMap, std::string_view resourcePath)
    : HdRenderDelegate(settingsMap)
    , _resourcePath(resourcePath)
    , _resourceRegistry(std::make_shared<HdResourceRegistry>())
    , _renderParam(std::make_unique<HdRobotRenderParam>())
{
  _settingDescriptors.emplace_back(HdRenderSettingDescriptor{"Samples Per Pixel", HdRobotSettingsTokens->spp, VtValue(4)});
  _settingDescriptors.emplace_back(
      HdRenderSettingDescriptor{"Enable DLSS-RR Denoise", HdRobotSettingsTokens->dlssRRDenoise, VtValue(true)});
  _settingDescriptors.emplace_back(
      HdRenderSettingDescriptor{"Enable DLSS-SR Upscale", HdRobotSettingsTokens->dlssSREnable, VtValue(true)});
  _settingDescriptors.emplace_back(
      HdRenderSettingDescriptor{"DLSS-SR Render Scale", HdRobotSettingsTokens->dlssSRScale, VtValue(0.6f)});
  _settingDescriptors.emplace_back(
      HdRenderSettingDescriptor{"Enable Lidar", HdRobotSettingsTokens->lidarEnable, VtValue(true)});

  if(_settingsMap.find(HdRobotSettingsTokens->spp) == _settingsMap.end())
  {
    _settingsMap[HdRobotSettingsTokens->spp] = VtValue(4);
  }
  if(_settingsMap.find(HdRobotSettingsTokens->dlssRRDenoise) == _settingsMap.end())
  {
    _settingsMap[HdRobotSettingsTokens->dlssRRDenoise] = VtValue(true);
  }
  if(_settingsMap.find(HdRobotSettingsTokens->dlssSREnable) == _settingsMap.end())
  {
    _settingsMap[HdRobotSettingsTokens->dlssSREnable] = VtValue(true);
  }
  if(_settingsMap.find(HdRobotSettingsTokens->dlssSRScale) == _settingsMap.end())
  {
    _settingsMap[HdRobotSettingsTokens->dlssSRScale] = VtValue(0.6f);
  }
  if(_settingsMap.find(HdRobotSettingsTokens->lidarEnable) == _settingsMap.end())
  {
    _settingsMap[HdRobotSettingsTokens->lidarEnable] = VtValue(true);
  }
}

HdRobotRenderDelegate::~HdRobotRenderDelegate() {}

HdRenderSettingDescriptorList HdRobotRenderDelegate::GetRenderSettingDescriptors() const
{
  return _settingDescriptors;
}

void HdRobotRenderDelegate::SetRenderSetting(const TfToken& key, const VtValue& value)
{
#ifdef NDEBUG
  // Disallow changing debug render settings in release config.
  for(const HdRenderSettingDescriptor& descriptor : _debugSettingDescriptors)
  {
    if(key == descriptor.key)
    {
      return;
    }
  }
#endif
  HdRenderDelegate::SetRenderSetting(key, value);
}

const HdCommandDescriptors COMMAND_DESCRIPTORS = {
    HdCommandDescriptor{HdRobotCommandTokens->printLicenses, "Print Licenses"}};

HdCommandDescriptors HdRobotRenderDelegate::GetCommandDescriptors() const
{
  return COMMAND_DESCRIPTORS;
}

bool HdRobotRenderDelegate::InvokeCommand(const TfToken& command, [[maybe_unused]] const HdCommandArgs& args)
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

    const char* licenseText = mapping.get();

    printf("%s\n", licenseText);
    fflush(stdout);

    return true;
  }

  TF_RUNTIME_ERROR("Unsupported command %s", command.GetText());

  return false;
}

HdRenderPassSharedPtr HdRobotRenderDelegate::CreateRenderPass(HdRenderIndex* index, const HdRprimCollection& collection)
{
  HdRobotRenderParam* robotRenderParam = _GetRobotRenderParam();
  if(!TF_VERIFY(robotRenderParam != nullptr))
  {
    return nullptr;
  }

  return HdRenderPassSharedPtr(new HdRobotRenderPass(index, collection, _settingsMap, *robotRenderParam, _resourcePath));
}

HdResourceRegistrySharedPtr HdRobotRenderDelegate::GetResourceRegistry() const
{
  return _resourceRegistry;
}

void HdRobotRenderDelegate::CommitResources(HdChangeTracker* tracker)
{
  TF_UNUSED(tracker);
  // We delay BVH building and GPU uploads to the next render call.
}

HdInstancer* HdRobotRenderDelegate::CreateInstancer(HdSceneDelegate* delegate, const SdfPath& id)
{
  return new HdRobotInstancer(delegate, id);
}

void HdRobotRenderDelegate::DestroyInstancer(HdInstancer* instancer)
{
  delete instancer;
}

HdAovDescriptor HdRobotRenderDelegate::GetDefaultAovDescriptor(const TfToken& name) const
{
  if(name == HdAovTokens->color)
  {
    return HdAovDescriptor(HdFormatFloat32Vec4, true, VtValue(GfVec4f(1.0f)));
  }
  else if(name == HdAovTokens->depth)
  {
    return HdAovDescriptor(HdFormatFloat32, false, VtValue(1.0f));
  }
  else if(name == HdAovTokens->primId || name == HdAovTokens->elementId || name == HdAovTokens->instanceId)
  {
    return HdAovDescriptor(HdFormatInt32, false, VtValue(-1));
  }
  else if(name == HdRobotAovTokens->dlssRRDiffuseAlbedo || name == HdRobotAovTokens->dlssRRSpecularAlbedo
          || name == HdRobotAovTokens->dlssRRNormalRoughness || name == HdRobotAovTokens->lidarPointCloud)
  {
    return HdAovDescriptor(HdFormatFloat32Vec4, true, VtValue(GfVec4f(0.0f)));
  }
  else if(name == HdRobotAovTokens->dlssRRMotionVector)
  {
    return HdAovDescriptor(HdFormatFloat32Vec2, true, VtValue(GfVec2f(0.0f)));
  }
  else if(name == HdRobotAovTokens->distanceToCamera || name == HdRobotAovTokens->dlssRRLinearDepth
          || name == HdRobotAovTokens->dlssRRSpecularHitDistance)
  {
    return HdAovDescriptor(HdFormatFloat32, true, VtValue(0.0f));
  }

  return HdAovDescriptor();
}

HdRenderParam* HdRobotRenderDelegate::GetRenderParam() const
{
  return _renderParam.get();
}

const TfTokenVector& HdRobotRenderDelegate::GetSupportedRprimTypes() const
{
  return _supportedRprimTypes;
}

HdRprim* HdRobotRenderDelegate::CreateRprim(const TfToken& typeId, const SdfPath& rprimId)
{
  const auto factoryIt = _rprimFactories.find(typeId);
  if(factoryIt != _rprimFactories.end())
  {
    HdRobotRenderParam* robotRenderParam = _GetRobotRenderParam();
    if(!TF_VERIFY(robotRenderParam != nullptr))
    {
      return nullptr;
    }
    return factoryIt->second(rprimId, *robotRenderParam);
  }

  return nullptr;
}

void HdRobotRenderDelegate::DestroyRprim(HdRprim* rprim)
{
  delete rprim;
}

const TfTokenVector& HdRobotRenderDelegate::GetSupportedSprimTypes() const
{
  return _supportedSprimTypes;
}

HdSprim* HdRobotRenderDelegate::CreateSprim(const TfToken& typeId, const SdfPath& sprimId)
{
  HdRobotRenderParam* robotRenderParam = _GetRobotRenderParam();
  if(!TF_VERIFY(robotRenderParam != nullptr))
  {
    return nullptr;
  }

  if(typeId == HdPrimTypeTokens->camera)
  {
    return new HdRobotCamera(sprimId);
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
  else if(typeId == HdPrimTypeTokens->domeLight)
  {
    return new HdRobotDomeLight(sprimId, *robotRenderParam);
  }
  return nullptr;
}

HdSprim* HdRobotRenderDelegate::CreateFallbackSprim(const TfToken& typeId)
{
  const SdfPath& sprimId = SdfPath::EmptyPath();

  return CreateSprim(typeId, sprimId);
}

void HdRobotRenderDelegate::DestroySprim(HdSprim* sprim)
{
  delete sprim;
}

const TfTokenVector& HdRobotRenderDelegate::GetSupportedBprimTypes() const
{
  return _supportedBprimTypes;
}

HdBprim* HdRobotRenderDelegate::CreateBprim(const TfToken& typeId, const SdfPath& bprimId)
{
  if(typeId == HdPrimTypeTokens->renderBuffer)
  {
    return new HdRobotRenderBuffer(bprimId, this);
  }

  return nullptr;
}

HdBprim* HdRobotRenderDelegate::CreateFallbackBprim(const TfToken& typeId)
{
  const SdfPath& bprimId = SdfPath::EmptyPath();

  return CreateBprim(typeId, bprimId);
}

void HdRobotRenderDelegate::DestroyBprim(HdBprim* bprim)
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
bool HdRobotRenderDelegate::IsParallelSyncEnabled(const TfToken& primType) const
{
  return primType == HdPrimTypeTokens->mesh || primType == HdPrimTypeTokens->material || primType == HdPrimTypeTokens->instancer;
}
#endif

Hgi* HdRobotRenderDelegate::GetHgi()
{
  return _hgi;
}

void HdRobotRenderDelegate::SetDrivers(HdDriverVector const& drivers)
{
  // For Storm we want to use the Hgi driver, so extract it.
  for(HdDriver* hdDriver : drivers)
  {
    if(hdDriver->name == HgiTokens->renderDriver && hdDriver->driver.IsHolding<Hgi*>())
    {
      _hgi = hdDriver->driver.UncheckedGet<Hgi*>();
      break;
    }
  }

  TF_VERIFY(_hgi, "HdSt requires Hgi HdDriver");
}

HdRobotRenderParam* HdRobotRenderDelegate::_GetRobotRenderParam() const
{
  return _renderParam.get();
}

PXR_NAMESPACE_CLOSE_SCOPE

