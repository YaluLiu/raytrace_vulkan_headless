#pragma once

#include <algorithm>
#include <cstdint>

#include <vulkan/vulkan_core.h>

enum class HeadlessTileRenderMode {
  Serial,
  MultiviewPreferred,
  MultiviewOnly,
};

struct HeadlessTileConfig {
  bool enabled = false;
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

  bool operator==(const HeadlessTileConfig &rhs) const {
    return enabled == rhs.enabled && cameraWidth == rhs.cameraWidth &&
           cameraHeight == rhs.cameraHeight && gridColumns == rhs.gridColumns &&
           gridRows == rhs.gridRows;
  }

  bool operator!=(const HeadlessTileConfig &rhs) const {
    return !(*this == rhs);
  }
};
