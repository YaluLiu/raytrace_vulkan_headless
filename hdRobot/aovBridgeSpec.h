#pragma once

#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/renderPassState.h>

#include <engine/aov_texture.hpp>
#include <engine/tile_config.hpp>

#include <optional>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

enum class HdRobotAovStorageRole
{
  Color,
  Depth,
  Id,
};

enum class HdRobotAovTileKind
{
  None,
  FixedSize,
  Display,
};

enum class HdRobotAovCopyScaling
{
  RequireExactSourceSize,
  AllowScaleToDestination,
};

struct HdRobotAovSpec
{
  Aov engineAov;
  HdFormat hdFormat;
  HdRobotAovStorageRole storageRole;
  // Tile depth atlas copies use R32F color-compatible textures, not GL depth attachments.
  bool useDepthTargetRenderBuffer;
  HdRobotAovTileKind tileKind;
  TileAovChannelMask tileChannels;
  HdRobotAovCopyScaling copyScaling;
  int copyPriority;
  bool multiSampled;
  VtValue clearValue;
};

std::optional<HdRobotAovSpec> GetHdRobotAovSpec(const TfToken &name);
HdAovDescriptor GetHdRobotDefaultAovDescriptor(const TfToken &name);
TileAovChannelMask ComputeHdRobotRequestedTileAovChannels(const HdRenderPassAovBindingVector &bindings);
bool IsHdRobotFixedSizeTileAov(const TfToken &name);
bool IsHdRobotDisplayTileAov(const TfToken &name);
bool IsHdRobotIgnoredAov(const TfToken &name);
int GetHdRobotAovCopyPriority(const TfToken &name);
std::vector<const HdRenderPassAovBinding *> GetHdRobotAovCopyOrder(const HdRenderPassAovBindingVector &bindings);

PXR_NAMESPACE_CLOSE_SCOPE
