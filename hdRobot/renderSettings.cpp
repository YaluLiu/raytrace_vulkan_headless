#include "renderSettings.h"

#include "hello_vulkan.hpp"
#include "tokens.h"

#include <pxr/base/vt/value.h>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
bool TryGetSettingValue(const HdRenderSettingsMap& settings, const TfToken& token, const VtValue** value)
{
  const auto it = settings.find(token);
  if(it == settings.end())
  {
    return false;
  }

  *value = &it->second;
  return true;
}

bool TryConvertSpp(const VtValue& value, int* out)
{
  if(value.IsHolding<int>())
  {
    *out = value.UncheckedGet<int>();
    return true;
  }
  if(value.IsHolding<unsigned int>())
  {
    *out = static_cast<int>(value.UncheckedGet<unsigned int>());
    return true;
  }
  if(value.IsHolding<float>())
  {
    *out = static_cast<int>(value.UncheckedGet<float>());
    return true;
  }
  if(value.IsHolding<double>())
  {
    *out = static_cast<int>(value.UncheckedGet<double>());
    return true;
  }
  return false;
}

bool TryConvertDlssEnable(const VtValue& value, bool* out)
{
  if(value.IsHolding<bool>())
  {
    *out = value.UncheckedGet<bool>();
    return true;
  }
  if(value.IsHolding<int>())
  {
    *out = value.UncheckedGet<int>() != 0;
    return true;
  }
  return false;
}

bool TryConvertDlssScale(const VtValue& value, float* out)
{
  if(value.IsHolding<float>())
  {
    *out = value.UncheckedGet<float>();
    return true;
  }
  if(value.IsHolding<double>())
  {
    *out = static_cast<float>(value.UncheckedGet<double>());
    return true;
  }
  if(value.IsHolding<int>())
  {
    *out = static_cast<float>(value.UncheckedGet<int>());
    return true;
  }
  if(value.IsHolding<unsigned int>())
  {
    *out = static_cast<float>(value.UncheckedGet<unsigned int>());
    return true;
  }
  return false;
}

bool TryConvertLidarEnable(const VtValue& value, bool* out)
{
  if(value.IsHolding<bool>())
  {
    *out = value.UncheckedGet<bool>();
    return true;
  }
  if(value.IsHolding<int>())
  {
    *out = value.UncheckedGet<int>() != 0;
    return true;
  }
  if(value.IsHolding<unsigned int>())
  {
    *out = value.UncheckedGet<unsigned int>() != 0;
    return true;
  }
  if(value.IsHolding<float>())
  {
    *out = value.UncheckedGet<float>() != 0.0f;
    return true;
  }
  if(value.IsHolding<double>())
  {
    *out = value.UncheckedGet<double>() != 0.0;
    return true;
  }
  return false;
}
}  // namespace

void ApplyRenderSettingsToApp(const HdRenderSettingsMap& settings, ::HelloVulkan& app)
{
  const VtValue* value = nullptr;
  int            spp   = 0;
  if(TryGetSettingValue(settings, HdRobotSettingsTokens->spp, &value) && TryConvertSpp(*value, &spp))
  {
    app.setSamplesPerFrame(spp);
  }

  bool enabled = false;
  if(TryGetSettingValue(settings, HdRobotSettingsTokens->dlssRRDenoise, &value) && TryConvertDlssEnable(*value, &enabled))
  {
    app.setDlssRREnabled(enabled);
  }

  if(TryGetSettingValue(settings, HdRobotSettingsTokens->dlssSREnable, &value) && TryConvertDlssEnable(*value, &enabled))
  {
    app.setDlssSREnabled(enabled);
  }

  float scale = 0.0f;
  if(TryGetSettingValue(settings, HdRobotSettingsTokens->dlssSRScale, &value) && TryConvertDlssScale(*value, &scale))
  {
    app.setDlssSRScale(scale);
  }
}

bool GetLidarEnabledSetting(const HdRenderSettingsMap& settings)
{
  const VtValue* value = nullptr;
  bool           enabled = true;
  if(TryGetSettingValue(settings, HdRobotSettingsTokens->lidarEnable, &value))
  {
    TryConvertLidarEnable(*value, &enabled);
  }
  return enabled;
}

PXR_NAMESPACE_CLOSE_SCOPE
