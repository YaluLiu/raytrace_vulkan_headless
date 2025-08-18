//
// Copyright (C) 2019-2022 Pablo Delgado Krämer
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
  HdRendererPluginRegistry::Define<HdGatlingRendererPlugin>();
}

HdGatlingRendererPlugin::HdGatlingRendererPlugin()
{
}

HdGatlingRendererPlugin::~HdGatlingRendererPlugin()
{
}

HdRenderDelegate* HdGatlingRendererPlugin::CreateRenderDelegate()
{
  HdRenderSettingsMap settingsMap;

  return CreateRenderDelegate(settingsMap);
}

HdRenderDelegate* HdGatlingRendererPlugin::CreateRenderDelegate(const HdRenderSettingsMap& settingsMap)
{
  PlugPluginPtr plugin = PLUG_THIS_PLUGIN;

  const std::string& resourcePath = plugin->GetResourcePath();

  return new HdGatlingRenderDelegate(settingsMap, resourcePath);
}

void HdGatlingRendererPlugin::DeleteRenderDelegate(HdRenderDelegate* renderDelegate)
{
  delete renderDelegate;
}

#if PXR_VERSION >= 2302
bool HdGatlingRendererPlugin::IsSupported(bool gpuEnabled) const
#else
bool HdGatlingRendererPlugin::IsSupported() const
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
