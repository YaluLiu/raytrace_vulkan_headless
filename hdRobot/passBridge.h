#pragma once

#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/renderPass.h>
#include <pxr/imaging/hd/renderPassState.h>
#include <pxr/base/gf/vec2i.h>

PXR_NAMESPACE_OPEN_SCOPE

namespace hdrobot {

class EngineSession;

struct FrameRenderResult
{
  bool frameRendered = false;
  bool aovsCopied = false;

  bool IsConverged() const { return frameRendered && aovsCopied; }
};

class PassBridge final
{
public:
  explicit PassBridge(EngineSession& engineSession);
  ~PassBridge();

  FrameRenderResult RenderFrameAndCopyAovs(const HdRenderPassStateSharedPtr& renderPassState,
                                                  const TfTokenVector& renderTags);

private:
  void configureRequestedTileAovChannels(const HdRenderPassAovBindingVector& hdAovBindings);
  bool copyRenderedAovs(const HdRenderPassAovBindingVector& hdAovBindings);

  EngineSession& _engineSession;
};

} // namespace hdrobot

PXR_NAMESPACE_CLOSE_SCOPE
