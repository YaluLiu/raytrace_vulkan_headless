#pragma once

#include <optional>
#include <vulkan/vulkan_core.h>

enum class RasterAov
{
  Color,
  PrimId,
  InstanceId,
  Depth,
  TileColor,
  TileDepth,
};

struct ExportedRasterAovTexture
{
  VkDevice       device{VK_NULL_HANDLE};
  VkImage        image{VK_NULL_HANDLE};
  VkImageView    imageView{VK_NULL_HANDLE};
  VkDeviceMemory memory{VK_NULL_HANDLE};
  VkDeviceSize   memoryOffset{0};
  VkDeviceSize   memorySize{0};
  VkFormat       format{VK_FORMAT_UNDEFINED};
  VkExtent2D     extent{0, 0};
  VkImageLayout  layout{VK_IMAGE_LAYOUT_GENERAL};
  bool           dedicatedMemory{false};
};
