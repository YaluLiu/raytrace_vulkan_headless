#include "aovBridgeSpec.h"

#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/pxr.h>

#include "tokens.h"

#include <algorithm>
#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
constexpr int kFixedTileCopyPriority = 0;
constexpr int kDisplayTileCopyPriority = 1;
constexpr int kStandardCopyPriority = 2;

hdrobot::AovSpec MakeSpec(Aov engineAov,
                        HdFormat hdFormat,
                        hdrobot::AovStorageRole storageRole,
                        bool useDepthTargetRenderBuffer,
                        hdrobot::AovTileKind tileKind,
                        TileAovChannelMask tileChannels,
                        hdrobot::AovCopyScaling copyScaling,
                        int copyPriority,
                        bool multiSampled,
                        VtValue clearValue)
{
  return hdrobot::AovSpec{
      engineAov,
      hdFormat,
      storageRole,
      useDepthTargetRenderBuffer,
      tileKind,
      tileChannels,
      copyScaling,
      copyPriority,
      multiSampled,
      std::move(clearValue),
  };
}
} // namespace

std::optional<hdrobot::AovSpec> GetHdRobotAovSpec(const TfToken &name)
{
  if(name == HdAovTokens->color)
  {
    return MakeSpec(Aov::Color, HdFormatFloat32Vec4, hdrobot::AovStorageRole::Color,
                    false, hdrobot::AovTileKind::None, TileAovChannelMask::None(),
                    hdrobot::AovCopyScaling::AllowScaleToDestination, kStandardCopyPriority, true,
                    VtValue(GfVec4f(1.0f)));
  }
  if(name == HdAovTokens->primId)
  {
    return MakeSpec(Aov::PrimId, HdFormatInt32, hdrobot::AovStorageRole::Id,
                    false, hdrobot::AovTileKind::None, TileAovChannelMask::None(),
                    hdrobot::AovCopyScaling::AllowScaleToDestination, kStandardCopyPriority, false, VtValue(-1));
  }
  if(name == HdAovTokens->instanceId)
  {
    return MakeSpec(Aov::InstanceId, HdFormatInt32, hdrobot::AovStorageRole::Id,
                    false, hdrobot::AovTileKind::None, TileAovChannelMask::None(),
                    hdrobot::AovCopyScaling::AllowScaleToDestination, kStandardCopyPriority, false, VtValue(-1));
  }
  if(name == HdAovTokens->depth || name == HdAovTokens->depthStencil)
  {
    return MakeSpec(Aov::Depth, HdFormatFloat32, hdrobot::AovStorageRole::Depth,
                    true, hdrobot::AovTileKind::None, TileAovChannelMask::None(),
                    hdrobot::AovCopyScaling::AllowScaleToDestination, kStandardCopyPriority, false, VtValue(1.0f));
  }
  if(name == HdRobotAovTokens->tileColor)
  {
    return MakeSpec(Aov::TileColor, HdFormatFloat32Vec4, hdrobot::AovStorageRole::Color,
                    false, hdrobot::AovTileKind::FixedSize, TileAovChannelMask::FromChannel(TileAovChannel::Color),
                    hdrobot::AovCopyScaling::RequireExactSourceSize, kFixedTileCopyPriority, true,
                    VtValue(GfVec4f(1.0f)));
  }
  if(name == HdRobotAovTokens->tileDepth)
  {
    return MakeSpec(Aov::TileDepth, HdFormatFloat32, hdrobot::AovStorageRole::Depth,
                    false, hdrobot::AovTileKind::FixedSize, TileAovChannelMask::FromChannel(TileAovChannel::Depth),
                    hdrobot::AovCopyScaling::RequireExactSourceSize, kFixedTileCopyPriority, false, VtValue(1.0f));
  }
  if(name == HdRobotAovTokens->tileDisplayColor)
  {
    return MakeSpec(Aov::TileColor, HdFormatFloat32Vec4, hdrobot::AovStorageRole::Color,
                    false, hdrobot::AovTileKind::Display, TileAovChannelMask::FromChannel(TileAovChannel::Color),
                    hdrobot::AovCopyScaling::AllowScaleToDestination, kDisplayTileCopyPriority, true,
                    VtValue(GfVec4f(1.0f)));
  }
  if(name == HdRobotAovTokens->tileDisplayDepth)
  {
    return MakeSpec(Aov::TileDepth, HdFormatFloat32, hdrobot::AovStorageRole::Depth,
                    false, hdrobot::AovTileKind::Display, TileAovChannelMask::FromChannel(TileAovChannel::Depth),
                    hdrobot::AovCopyScaling::AllowScaleToDestination, kDisplayTileCopyPriority, false, VtValue(1.0f));
  }
  return std::nullopt;
}

HdAovDescriptor GetHdRobotDefaultAovDescriptor(const TfToken &name)
{
  const std::optional<hdrobot::AovSpec> spec = GetHdRobotAovSpec(name);
  if(!spec)
  {
    return HdAovDescriptor();
  }
  return HdAovDescriptor(spec->hdFormat, spec->multiSampled, spec->clearValue);
}

TileAovChannelMask ComputeHdRobotRequestedTileAovChannels(const HdRenderPassAovBindingVector &bindings)
{
  TileAovChannelMask channels = TileAovChannelMask::None();
  for(const HdRenderPassAovBinding &binding : bindings)
  {
    if(binding.renderBuffer == nullptr)
    {
      continue;
    }

    const std::optional<hdrobot::AovSpec> spec = GetHdRobotAovSpec(binding.aovName);
    if(spec && spec->tileChannels.any())
    {
      channels |= spec->tileChannels;
    }
  }
  return channels;
}

bool IsHdRobotFixedSizeTileAov(const TfToken &name)
{
  const std::optional<hdrobot::AovSpec> spec = GetHdRobotAovSpec(name);
  return spec && spec->tileKind == hdrobot::AovTileKind::FixedSize;
}

bool IsHdRobotDisplayTileAov(const TfToken &name)
{
  const std::optional<hdrobot::AovSpec> spec = GetHdRobotAovSpec(name);
  return spec && spec->tileKind == hdrobot::AovTileKind::Display;
}

bool IsHdRobotIgnoredAov(const TfToken &name)
{
#if PXR_VERSION >= 2408
  return name == HdAovTokens->elementId;
#else
  (void)name;
  return false;
#endif
}

int GetHdRobotAovCopyPriority(const TfToken &name)
{
  const std::optional<hdrobot::AovSpec> spec = GetHdRobotAovSpec(name);
  return spec ? spec->copyPriority : kStandardCopyPriority;
}

std::vector<const HdRenderPassAovBinding *> GetHdRobotAovCopyOrder(const HdRenderPassAovBindingVector &bindings)
{
  std::vector<const HdRenderPassAovBinding *> copyOrder;
  copyOrder.reserve(bindings.size());
  for(const HdRenderPassAovBinding &binding : bindings)
  {
    copyOrder.push_back(&binding);
  }
  std::stable_sort(copyOrder.begin(), copyOrder.end(), [](const HdRenderPassAovBinding *lhs,
                                                          const HdRenderPassAovBinding *rhs) {
    return GetHdRobotAovCopyPriority(lhs->aovName) < GetHdRobotAovCopyPriority(rhs->aovName);
  });
  return copyOrder;
}

PXR_NAMESPACE_CLOSE_SCOPE
