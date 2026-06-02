#pragma once

#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/renderPass.h>
#include <pxr/imaging/hd/renderPassState.h>
#include <pxr/base/gf/vec2i.h>

PXR_NAMESPACE_OPEN_SCOPE

class HdRobotRenderResources;

class HdRobotRenderBridge final
{
public:
  explicit HdRobotRenderBridge(HdRobotRenderResources& resources);
  ~HdRobotRenderBridge();

  bool RenderFrame(const HdRenderPassStateSharedPtr& renderPassState, const TfTokenVector& renderTags);

private:
  void configureFrameOutputs(const HdRenderPassAovBindingVector& hdAovBindings);
  bool copyRenderedAovs(const HdRenderPassAovBindingVector& hdAovBindings);

  HdRobotRenderResources& _resources;
};

PXR_NAMESPACE_CLOSE_SCOPE
