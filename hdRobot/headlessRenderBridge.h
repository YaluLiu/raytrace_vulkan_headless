//
// Copyright (C) 2023 Pablo Delgado Kramer
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
