#pragma once

#include "glInteropCache.h"
#include "renderParam.h"

#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/renderPass.h>
#include <pxr/imaging/hd/renderPassState.h>
#include <pxr/base/gf/vec2i.h>

#include <raster/raster_session.hpp>

#include <string>

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRasterBridge final
{
public:
  HdRobotRasterBridge(HdRobotRenderParam& renderParam, std::string resourcePath);
  ~HdRobotRasterBridge();

  bool RenderRasterFrame(const HdRenderPassStateSharedPtr& renderPassState, const TfTokenVector& renderTags);

private:
  void ensureRasterSessionReady(const GfVec2i& renderSize);
  void initializeRasterSession(const GfVec2i& renderSize);
  void resizeRasterSession(const GfVec2i& renderSize);
  void uploadInitialScene();
  void refreshTextureAssetsIfNeeded();
  bool updateCameras(const HdRenderPassStateSharedPtr& renderPassState);
  void updateLights();
  void updateScene();
  void updateGeometry();
  void updateInstances();
  void updateMaterials();
  bool updateActiveRenderTags(const TfTokenVector& renderTags);
  bool isMeshRenderTagMatched(const HydraMesh& mesh) const;

  HdRobotRenderParam&        _renderParam;
  std::string                _resourcePath;

  RasterSession            _rasterSession;
  HdRobotGlInteropCache  _glInteropCache;
  bool          _isAppInited = false;
  TfTokenVector _activeRenderTags;
  GfVec2i       _renderSize{-1, -1};
  uint64_t      _uploadedTextureRegistryVersion = 0;
};

PXR_NAMESPACE_CLOSE_SCOPE
