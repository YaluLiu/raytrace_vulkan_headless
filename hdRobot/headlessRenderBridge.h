#pragma once

#include "renderParam.h"

#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/renderPass.h>
#include <pxr/imaging/hd/renderPassState.h>

#include <ray_trace_app.hpp>

#include <string>

PXR_NAMESPACE_OPEN_SCOPE

class HeadlessRenderBridge final
{
public:
  HeadlessRenderBridge(const HdRenderSettingsMap& settings, HdRobotRenderParam& renderParam, std::string resourcePath);

  bool RenderFrame(const HdRenderPassStateSharedPtr& renderPassState, const TfTokenVector& renderTags);

private:
  void applyRenderSettings();
  void initOrResize();
  bool updateMainCamera(const HdRenderPassStateSharedPtr& renderPassState);
  void updateLidarCamera();
  void updateHeightScanCamera();
  void updateLights();
  void updateScene();
  void updateBlas();
  void updateTlas();
  void updateMaterials();
  bool updateActiveRenderTags(const TfTokenVector& renderTags);
  bool isMeshRenderTagMatched(const HydraMesh& mesh) const;

  const HdRenderSettingsMap& _settings;
  HdRobotRenderParam&        _renderParam;
  std::string                _resourcePath;

  RayTraceApp   _renderApp;
  bool          _isAppInited = false;
  bool          _resetRenderBuffer = false;
  TfTokenVector _activeRenderTags;
  int           _frameIndex = 0;
  int           _width = -1;
  int           _height = -1;
};

PXR_NAMESPACE_CLOSE_SCOPE
