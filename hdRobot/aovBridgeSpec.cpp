#include "aovBridgeSpec.h"

#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/pxr.h>

#include "tokens.h"

#include <utility>

PXR_NAMESPACE_OPEN_SCOPE

namespace
{
constexpr int kFixedTileCopyPriority = 0;
constexpr int kDisplayTileCopyPriority = 1;
constexpr int kStandardCopyPriority = 2;

HdRobotAovSpec MakeSpec(RasterAov rasterAov,
                        HdFormat hdFormat,
                        HdRobotAovStorageRole storageRole,
                        bool useDepthTargetRenderBuffer,
                        HdRobotAovTileKind tileKind,
                        TileAovChannelMask tileChannels,
                        HdRobotAovCopyScaling copyScaling,
                        int copyPriority,
                        bool multiSampled,
                        VtValue clearValue)
{
  return HdRobotAovSpec{
      rasterAov,
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

std::optional<HdRobotAovSpec> GetHdRobotAovSpec(const TfToken &name)
{
  if(name == HdAovTokens->color)
  {
    return MakeSpec(RasterAov::Color, HdFormatFloat32Vec4, HdRobotAovStorageRole::Color,
                    false, HdRobotAovTileKind::None, TileAovChannelMask::None(),
                    HdRobotAovCopyScaling::AllowScaleToDestination, kStandardCopyPriority, true,
                    VtValue(GfVec4f(1.0f)));
  }
  if(name == HdAovTokens->primId)
  {
    return MakeSpec(RasterAov::PrimId, HdFormatInt32, HdRobotAovStorageRole::Id,
                    false, HdRobotAovTileKind::None, TileAovChannelMask::None(),
                    HdRobotAovCopyScaling::AllowScaleToDestination, kStandardCopyPriority, false, VtValue(-1));
  }
  if(name == HdAovTokens->instanceId)
  {
    return MakeSpec(RasterAov::InstanceId, HdFormatInt32, HdRobotAovStorageRole::Id,
                    false, HdRobotAovTileKind::None, TileAovChannelMask::None(),
                    HdRobotAovCopyScaling::AllowScaleToDestination, kStandardCopyPriority, false, VtValue(-1));
  }
  if(name == HdAovTokens->depth || name == HdAovTokens->depthStencil)
  {
    return MakeSpec(RasterAov::Depth, HdFormatFloat32, HdRobotAovStorageRole::Depth,
                    true, HdRobotAovTileKind::None, TileAovChannelMask::None(),
                    HdRobotAovCopyScaling::AllowScaleToDestination, kStandardCopyPriority, false, VtValue(1.0f));
  }
  if(name == HdRobotAovTokens->tileColor)
  {
    return MakeSpec(RasterAov::TileColor, HdFormatFloat32Vec4, HdRobotAovStorageRole::Color,
                    false, HdRobotAovTileKind::FixedSize, TileAovChannelMask::FromChannel(TileAovChannel::Color),
                    HdRobotAovCopyScaling::RequireExactSourceSize, kFixedTileCopyPriority, true,
                    VtValue(GfVec4f(1.0f)));
  }
  if(name == HdRobotAovTokens->tileDepth)
  {
    return MakeSpec(RasterAov::TileDepth, HdFormatFloat32, HdRobotAovStorageRole::Depth,
                    false, HdRobotAovTileKind::FixedSize, TileAovChannelMask::FromChannel(TileAovChannel::Depth),
                    HdRobotAovCopyScaling::RequireExactSourceSize, kFixedTileCopyPriority, false, VtValue(1.0f));
  }
  if(name == HdRobotAovTokens->tileDisplayColor)
  {
    return MakeSpec(RasterAov::TileColor, HdFormatFloat32Vec4, HdRobotAovStorageRole::Color,
                    false, HdRobotAovTileKind::Display, TileAovChannelMask::FromChannel(TileAovChannel::Color),
                    HdRobotAovCopyScaling::AllowScaleToDestination, kDisplayTileCopyPriority, true,
                    VtValue(GfVec4f(1.0f)));
  }
  if(name == HdRobotAovTokens->tileDisplayDepth)
  {
    return MakeSpec(RasterAov::TileDepth, HdFormatFloat32, HdRobotAovStorageRole::Depth,
                    false, HdRobotAovTileKind::Display, TileAovChannelMask::FromChannel(TileAovChannel::Depth),
                    HdRobotAovCopyScaling::AllowScaleToDestination, kDisplayTileCopyPriority, false, VtValue(1.0f));
  }
  return std::nullopt;
}

HdAovDescriptor GetHdRobotDefaultAovDescriptor(const TfToken &name)
{
  const std::optional<HdRobotAovSpec> spec = GetHdRobotAovSpec(name);
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

    const std::optional<HdRobotAovSpec> spec = GetHdRobotAovSpec(binding.aovName);
    if(spec && spec->tileChannels.any())
    {
      channels |= spec->tileChannels;
    }
  }
  return channels;
}

bool IsHdRobotFixedSizeTileAov(const TfToken &name)
{
  const std::optional<HdRobotAovSpec> spec = GetHdRobotAovSpec(name);
  return spec && spec->tileKind == HdRobotAovTileKind::FixedSize;
}

bool IsHdRobotDisplayTileAov(const TfToken &name)
{
  const std::optional<HdRobotAovSpec> spec = GetHdRobotAovSpec(name);
  return spec && spec->tileKind == HdRobotAovTileKind::Display;
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
  const std::optional<HdRobotAovSpec> spec = GetHdRobotAovSpec(name);
  return spec ? spec->copyPriority : kStandardCopyPriority;
}

PXR_NAMESPACE_CLOSE_SCOPE
