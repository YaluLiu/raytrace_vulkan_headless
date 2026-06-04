#pragma once

#include <engine/tile_config.hpp>

#include <pxr/base/gf/vec2i.h>
#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/renderPassState.h>

#include <memory>
#include <string>

class Engine;
class HdRobotGlInteropCache;

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotBackendScene;
class HdRobotRenderParam;

class HdRobotRenderResources final
{
public:
  explicit HdRobotRenderResources(std::string resourcePath);
  ~HdRobotRenderResources();

  ::Engine& GetEngine();
  const ::Engine& GetEngine() const;
  ::HdRobotGlInteropCache& GetGlInteropCache();

  void EnsureEngineReady(const GfVec2i& renderSize);
  void CommitResources(HdRobotRenderParam& renderParam, HdRobotBackendScene& backendScene);

  bool SetFrameCamera(const HdRenderPassStateSharedPtr& renderPassState);
  void ConfigureFrameOutputs(TileAovChannelMask requestedTileChannels);
  void ApplyFrameRenderTags(const TfTokenVector& renderTags);

private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
};

PXR_NAMESPACE_CLOSE_SCOPE
