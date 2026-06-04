#pragma once

#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/renderPass.h>
#include <pxr/imaging/hd/renderPassState.h>
#include <pxr/base/gf/vec2i.h>

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotEngineSession;

struct HdRobotFrameRenderResult
{
  bool frameRendered = false;
  bool aovsCopied = false;

  bool IsConverged() const { return frameRendered && aovsCopied; }
};

class HdRobotPassBridge final
{
public:
  explicit HdRobotPassBridge(HdRobotEngineSession& engineSession);
  ~HdRobotPassBridge();

  HdRobotFrameRenderResult RenderFrameAndCopyAovs(const HdRenderPassStateSharedPtr& renderPassState,
                                                  const TfTokenVector& renderTags);

private:
  void configureRequestedTileAovChannels(const HdRenderPassAovBindingVector& hdAovBindings);
  bool copyRenderedAovs(const HdRenderPassAovBindingVector& hdAovBindings);

  HdRobotEngineSession& _engineSession;
};

PXR_NAMESPACE_CLOSE_SCOPE
