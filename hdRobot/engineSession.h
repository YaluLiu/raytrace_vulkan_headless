#pragma once

#include <engine/tile_config.hpp>

#include <pxr/base/gf/vec2i.h>
#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/renderPassState.h>

#include <memory>
#include <string>

class Engine;

PXR_NAMESPACE_OPEN_SCOPE

namespace hdrobot {

class GlInteropCache;
class SceneStore;
class RenderParam;

class EngineSession final
{
public:
  explicit EngineSession(std::string resourcePath);
  ~EngineSession();

  ::Engine& GetEngine();
  const ::Engine& GetEngine() const;
  GlInteropCache& GetGlInteropCache();

  void EnsureEngineReady(const GfVec2i& renderSize);
  void CommitResources(RenderParam& renderParam, SceneStore& sceneStore);

  bool SetFrameCamera(const HdRenderPassStateSharedPtr& renderPassState);
  void ConfigureFrameOutputs(TileAovChannelMask requestedTileChannels);
  void ApplyFrameRenderTags(const TfTokenVector& renderTags);

private:
  struct Impl;
  std::unique_ptr<Impl> _impl;
};

} // namespace hdrobot

PXR_NAMESPACE_CLOSE_SCOPE
