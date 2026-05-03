#include "rendererPlugin.h"
#include "renderDelegate.h"

#include <pxr/imaging/hdMtlx/hdMtlx.h>
#include <pxr/imaging/hd/rendererPluginRegistry.h>
#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/thisPlugin.h>
#include <pxr/usd/ar/asset.h>
#include <pxr/usd/ar/resolver.h>
#include <pxr/usd/ar/resolvedPath.h>
#include <pxr/usd/usdMtlx/utils.h>

#include <MaterialXCore/Document.h>
#include <MaterialXFormat/File.h>
#include <MaterialXFormat/Util.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfType)
{
  HdRendererPluginRegistry::Define<HdRobotRendererPlugin>();
}

HdRobotRendererPlugin::HdRobotRendererPlugin() {}

HdRobotRendererPlugin::~HdRobotRendererPlugin() {}

HdRenderDelegate* HdRobotRendererPlugin::CreateRenderDelegate()
{
  HdRenderSettingsMap settingsMap;

  return CreateRenderDelegate(settingsMap);
}

HdRenderDelegate* HdRobotRendererPlugin::CreateRenderDelegate(const HdRenderSettingsMap& settingsMap)
{
  PlugPluginPtr plugin = PLUG_THIS_PLUGIN;

  const std::string& resourcePath = plugin->GetResourcePath();

  return new HdRobotRenderDelegate(settingsMap, resourcePath);
}

void HdRobotRendererPlugin::DeleteRenderDelegate(HdRenderDelegate* renderDelegate)
{
  delete renderDelegate;
}

#if PXR_VERSION >= 2302
bool HdRobotRendererPlugin::IsSupported(bool gpuEnabled) const
#else
bool HdRobotRendererPlugin::IsSupported() const
#endif
{
  // Note: we just assume that the renderer is supported on the system here because usdview
  // (and possibly other applications) instantiate the renderer plugin multiple times,
  // checking for support.
  //
  // As performing a real GPU capability check here or in the constructor would at least double
  // the loading time, we instead assume support and do the actual check when the render delegate
  // is requested. Thankfully, returning null in case it's not supported has the same effect as
  // returning false in this function.
  return gpuEnabled;
}

PXR_NAMESPACE_CLOSE_SCOPE
