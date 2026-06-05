#pragma once

#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/imaging/hd/renderPassState.h>

#include <engine/aov_texture.hpp>
#include <engine/tile_config.hpp>

#include <optional>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

namespace hdrobot {

enum class AovStorageRole
{
  Color,
  Depth,
  Id,
};

enum class AovTileKind
{
  None,
  FixedSize,
  Display,
};

enum class AovCopyScaling
{
  RequireExactSourceSize,
  AllowScaleToDestination,
};

struct AovSpec
{
  Aov engineAov;
  HdFormat hdFormat;
  AovStorageRole storageRole;
  // Tile depth atlas copies use R32F color-compatible textures, not GL depth attachments.
  bool useDepthTargetRenderBuffer;
  AovTileKind tileKind;
  TileAovChannelMask tileChannels;
  AovCopyScaling copyScaling;
  int copyPriority;
  bool multiSampled;
  VtValue clearValue;
};

} // namespace hdrobot

std::optional<hdrobot::AovSpec> GetHdRobotAovSpec(const TfToken &name);
HdAovDescriptor GetHdRobotDefaultAovDescriptor(const TfToken &name);
TileAovChannelMask ComputeHdRobotRequestedTileAovChannels(const HdRenderPassAovBindingVector &bindings);
bool IsHdRobotFixedSizeTileAov(const TfToken &name);
bool IsHdRobotDisplayTileAov(const TfToken &name);
bool IsHdRobotIgnoredAov(const TfToken &name);
int GetHdRobotAovCopyPriority(const TfToken &name);
std::vector<const HdRenderPassAovBinding *> GetHdRobotAovCopyOrder(const HdRenderPassAovBindingVector &bindings);

PXR_NAMESPACE_CLOSE_SCOPE
