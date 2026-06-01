#pragma once

#include <algorithm>
#include <cstdint>

#include <vulkan/vulkan_core.h>

enum class TileAovChannel : uint32_t {
  Color = 1u << 0u,
  Depth = 1u << 1u,
};

struct TileAovChannelMask {
  uint32_t bits = 0;

  static constexpr TileAovChannelMask None() { return {}; }
  static constexpr TileAovChannelMask ColorDepth() {
    return {static_cast<uint32_t>(TileAovChannel::Color) | static_cast<uint32_t>(TileAovChannel::Depth)};
  }
  static constexpr TileAovChannelMask FromChannel(TileAovChannel channel) {
    return {static_cast<uint32_t>(channel)};
  }

  constexpr bool contains(TileAovChannel channel) const {
    return (bits & static_cast<uint32_t>(channel)) != 0;
  }
  constexpr bool any() const { return bits != 0; }
  constexpr bool none() const { return bits == 0; }

  constexpr TileAovChannelMask operator&(TileAovChannelMask rhs) const { return {bits & rhs.bits}; }
  constexpr TileAovChannelMask operator|(TileAovChannelMask rhs) const { return {bits | rhs.bits}; }
  constexpr TileAovChannelMask& operator&=(TileAovChannelMask rhs) {
    bits &= rhs.bits;
    return *this;
  }
  constexpr TileAovChannelMask& operator|=(TileAovChannelMask rhs) {
    bits |= rhs.bits;
    return *this;
  }
  constexpr bool operator==(TileAovChannelMask rhs) const { return bits == rhs.bits; }
  constexpr bool operator!=(TileAovChannelMask rhs) const { return !(*this == rhs); }
};

struct TileAtlasConfig {
  bool enabled = false;
  bool colorEnabled = true;
  bool depthEnabled = true;
  uint32_t cameraWidth = 1920;
  uint32_t cameraHeight = 1080;
  uint32_t gridColumns = 10;
  uint32_t gridRows = 10;

  void sanitize() {
    cameraWidth = std::max<uint32_t>(cameraWidth, 1);
    cameraHeight = std::max<uint32_t>(cameraHeight, 1);
    gridColumns = std::max<uint32_t>(gridColumns, 1);
    gridRows = std::max<uint32_t>(gridRows, 1);
  }

  VkExtent2D tileExtent() const { return {cameraWidth, cameraHeight}; }
  VkExtent2D atlasExtent() const {
    return {cameraWidth * gridColumns, cameraHeight * gridRows};
  }
  uint32_t capacity() const { return gridColumns * gridRows; }
  TileAovChannelMask enabledChannels() const {
    if(!enabled)
    {
      return TileAovChannelMask::None();
    }

    TileAovChannelMask channels = TileAovChannelMask::None();
    if(colorEnabled)
    {
      channels |= TileAovChannelMask::FromChannel(TileAovChannel::Color);
    }
    if(depthEnabled)
    {
      channels |= TileAovChannelMask::FromChannel(TileAovChannel::Depth);
    }
    return channels;
  }

  bool operator==(const TileAtlasConfig &rhs) const {
    return enabled == rhs.enabled && colorEnabled == rhs.colorEnabled && depthEnabled == rhs.depthEnabled &&
           cameraWidth == rhs.cameraWidth &&
           cameraHeight == rhs.cameraHeight && gridColumns == rhs.gridColumns &&
           gridRows == rhs.gridRows;
  }

  bool operator!=(const TileAtlasConfig &rhs) const {
    return !(*this == rhs);
  }
};
