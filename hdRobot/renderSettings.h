#pragma once

#include <pxr/imaging/hd/renderDelegate.h>

class HelloVulkan;

PXR_NAMESPACE_OPEN_SCOPE

bool RenderSettingsMayReallocateAovs(const HdRenderSettingsMap& settings, const ::HelloVulkan& app);
void ApplyRenderSettingsToApp(const HdRenderSettingsMap& settings, ::HelloVulkan& app);
bool GetLidarEnabledSetting(const HdRenderSettingsMap& settings);

PXR_NAMESPACE_CLOSE_SCOPE
