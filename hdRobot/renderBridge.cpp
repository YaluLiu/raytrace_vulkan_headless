#include "renderBridge.h"

#include <algorithm>
#include <iostream>
#include <optional>
#include <vector>

#include "aovBridgeSpec.h"
#include "hydraAovCopy.h"
#include "renderBuffer.h"
#include "renderResources.h"

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

HdRobotRenderBridge::HdRobotRenderBridge(HdRobotRenderResources& resources)
    : _resources(resources)
{
}

HdRobotRenderBridge::~HdRobotRenderBridge() = default;

bool HdRobotRenderBridge::RenderFrame(const HdRenderPassStateSharedPtr &renderPassState,
                                       const TfTokenVector &renderTags)
{
  if(!renderPassState)
  {
    return false;
  }

  const auto &hdAovBindings = renderPassState->GetAovBindings();
  if(!HasRenderBuffer(hdAovBindings))
  {
    return false;
  }

  const std::optional<GfVec2i> mainRenderSize = GetMainRenderSize(renderPassState, hdAovBindings);
  if(!mainRenderSize)
  {
    return false;
  }

  _resources.EnsureEngineReady(*mainRenderSize);
  if(!_resources.SetFrameCamera(renderPassState))
  {
    return false;
  }

  configureFrameOutputs(hdAovBindings);
  _resources.ApplyFrameRenderTags(renderTags);
  _resources.GetEngine().render();

  return copyRenderedAovs(hdAovBindings);
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
      copied = CopyAovToRenderBuffer(_resources.GetEngine(), request, _resources.GetGlInteropCache());
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

  std::vector<const HdRenderPassAovBinding *> copyOrder;
  copyOrder.reserve(hdAovBindings.size());
  for(const HdRenderPassAovBinding &binding : hdAovBindings)
  {
    copyOrder.push_back(&binding);
  }
  std::stable_sort(copyOrder.begin(), copyOrder.end(), [](const HdRenderPassAovBinding *lhs,
                                                          const HdRenderPassAovBinding *rhs) {
    return GetHdRobotAovCopyPriority(lhs->aovName) < GetHdRobotAovCopyPriority(rhs->aovName);
  });
  for(const HdRenderPassAovBinding *binding : copyOrder)
  {
    copyBinding(*binding);
  }

  return allAovsCopied;
}

void HdRobotRenderBridge::configureFrameOutputs(const HdRenderPassAovBindingVector &hdAovBindings)
{
  _resources.ConfigureFrameOutputs(ComputeHdRobotRequestedTileAovChannels(hdAovBindings));
}

PXR_NAMESPACE_CLOSE_SCOPE
