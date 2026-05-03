#pragma once

#include <pxr/imaging/hd/rendererPlugin.h>
#include <iostream>
#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRendererPlugin final : public HdRendererPlugin
{
public:
  HdRobotRendererPlugin();

  ~HdRobotRendererPlugin() override;

public:
  HdRenderDelegate* CreateRenderDelegate() override;

  HdRenderDelegate* CreateRenderDelegate(const HdRenderSettingsMap& settingsMap) override;

  void DeleteRenderDelegate(HdRenderDelegate* renderDelegate) override;

#if PXR_VERSION >= 2302
  bool IsSupported(bool gpuEnabled) const override;
#else
  bool IsSupported() const override;
#endif
};

PXR_NAMESPACE_CLOSE_SCOPE
