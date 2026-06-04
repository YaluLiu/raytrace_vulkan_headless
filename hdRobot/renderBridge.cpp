#include "renderBridge.h"

#include <algorithm>
#include <iostream>
#include <optional>
#include <vector>

#include "aovBridgeSpec.h"
#include "hydraAovCopy.h"
#include "renderBuffer.h"
#include "engineSession.h"

#include <engine/engine.hpp>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
bool HasRenderBuffer(const HdRenderPassAovBindingVector &bindings)
{
  return std::any_of(bindings.begin(), bindings.end(),
                     [](const HdRenderPassAovBinding &binding) { return binding.renderBuffer != nullptr; });
}

std::optional<GfVec2i> GetRenderBufferSize(const HdRenderPassAovBindingVector &bindings)
{
  for(const HdRenderPassAovBinding &binding : bindings)
  {
    if(binding.renderBuffer == nullptr || IsHdRobotFixedSizeTileAov(binding.aovName))
    {
      continue;
    }

    auto *renderBuffer = static_cast<HdRobotRenderBuffer *>(binding.renderBuffer);
    const int width = static_cast<int>(renderBuffer->GetWidth());
    const int height = static_cast<int>(renderBuffer->GetHeight());
    if(width > 0 && height > 0)
    {
      return GfVec2i(width, height);
    }
  }

  return std::nullopt;
}

std::optional<GfVec2i> GetMainRenderSize(const HdRenderPassStateSharedPtr &renderPassState,
                                         const HdRenderPassAovBindingVector &bindings)
{
  const CameraUtilFraming &framing = renderPassState->GetFraming();
  if(framing.IsValid())
  {
    const GfVec2i size = framing.dataWindow.GetSize();
    if(size[0] > 0 && size[1] > 0)
    {
      return size;
    }
  }

  const GfVec4f &viewport = renderPassState->GetViewport();
  const int viewportWidth = static_cast<int>(viewport[2]);
  const int viewportHeight = static_cast<int>(viewport[3]);
  if(viewportWidth > 0 && viewportHeight > 0)
  {
    return GfVec2i(viewportWidth, viewportHeight);
  }

  return GetRenderBufferSize(bindings);
}

} // namespace

HdRobotRenderBridge::HdRobotRenderBridge(HdRobotEngineSession& engineSession)
    : _engineSession(engineSession)
{
}

HdRobotRenderBridge::~HdRobotRenderBridge() = default;

HdRobotFrameRenderResult HdRobotRenderBridge::RenderFrameAndCopyAovs(const HdRenderPassStateSharedPtr &renderPassState,
                                                                     const TfTokenVector &renderTags)
{
  HdRobotFrameRenderResult result;
  if(!renderPassState)
  {
    return result;
  }

  const auto &hdAovBindings = renderPassState->GetAovBindings();
  if(!HasRenderBuffer(hdAovBindings))
  {
    return result;
  }

  const std::optional<GfVec2i> mainRenderSize = GetMainRenderSize(renderPassState, hdAovBindings);
  if(!mainRenderSize)
  {
    return result;
  }

  _engineSession.EnsureEngineReady(*mainRenderSize);
  if(!_engineSession.SetFrameCamera(renderPassState))
  {
    return result;
  }

  configureRequestedTileAovChannels(hdAovBindings);
  _engineSession.ApplyFrameRenderTags(renderTags);
  _engineSession.GetEngine().render();
  result.frameRendered = true;

  result.aovsCopied = copyRenderedAovs(hdAovBindings);
  return result;
}

bool HdRobotRenderBridge::copyRenderedAovs(const HdRenderPassAovBindingVector &hdAovBindings)
{
  bool allAovsCopied = true;

  auto copyBinding = [&](const HdRenderPassAovBinding &binding)
  {
    auto *aovBuffer = static_cast<HdRobotRenderBuffer *>(binding.renderBuffer);
    bool copied = false;
    const std::optional<HdRobotAovSpec> spec = GetHdRobotAovSpec(binding.aovName);
    if(spec)
    {
      const HdRobotAovCopyRequest request{binding.aovName, spec->engineAov, spec->copyScaling, aovBuffer};
      copied = CopyAovToRenderBuffer(_engineSession.GetEngine(), request, _engineSession.GetGlInteropCache());
    }
    else if(IsHdRobotIgnoredAov(binding.aovName))
    {
      copied = true;
    }
    else
    {
      std::cerr << "[HdRobotRenderBridge] Unsupported AOV token " << binding.aovName.GetString() << std::endl;
    }
    allAovsCopied = allAovsCopied && copied;
    if(aovBuffer != nullptr)
    {
      aovBuffer->SetConverged(copied);
    }
  };

  const std::vector<const HdRenderPassAovBinding *> copyOrder = GetHdRobotAovCopyOrder(hdAovBindings);
  for(const HdRenderPassAovBinding *binding : copyOrder)
  {
    copyBinding(*binding);
  }

  return allAovsCopied;
}

void HdRobotRenderBridge::configureRequestedTileAovChannels(const HdRenderPassAovBindingVector &hdAovBindings)
{
  _engineSession.ConfigureFrameOutputs(ComputeHdRobotRequestedTileAovChannels(hdAovBindings));
}

PXR_NAMESPACE_CLOSE_SCOPE
