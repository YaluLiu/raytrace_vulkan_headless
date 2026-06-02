#pragma once

#include "glInteropCache.h"
#include "renderParam.h"

#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/renderPass.h>
#include <pxr/imaging/hd/renderPassState.h>
#include <pxr/base/gf/vec2i.h>

#include <engine/engine.hpp>

#include <string>

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRenderBridge final
{
public:
  HdRobotRenderBridge(HdRobotRenderParam& renderParam, std::string resourcePath);
  ~HdRobotRenderBridge();

  bool RenderFrame(const HdRenderPassStateSharedPtr& renderPassState, const TfTokenVector& renderTags);

private:
  void ensureEngineReady(const GfVec2i& renderSize);
  void initializeEngine(const GfVec2i& renderSize);
  void resizeEngine(const GfVec2i& renderSize);
  void commitRendererResources();
  void uploadInitialScene();
  void refreshTextureAssetsIfNeeded();
  bool updateCameras(const HdRenderPassStateSharedPtr& renderPassState);
  void configureRendererOutputs(const HdRenderPassAovBindingVector& hdAovBindings);
  void updateLights();
  void updateGeometry();
  void updateInstances();
  void updateMaterials();
  bool updateActiveRenderTags(const TfTokenVector& renderTags);
  bool isMeshRenderTagMatched(const HydraMesh& mesh) const;

  HdRobotRenderParam&        _renderParam;
  std::string                _resourcePath;

  Engine            _engine;
  HdRobotGlInteropCache  _glInteropCache;
  bool          _isAppInited = false;
  TfTokenVector _activeRenderTags;
  GfVec2i       _renderSize{-1, -1};
  uint64_t      _uploadedTextureRegistryVersion = 0;
};

PXR_NAMESPACE_CLOSE_SCOPE
