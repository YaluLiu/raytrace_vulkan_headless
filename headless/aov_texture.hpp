#pragma once

#include <optional>
#include <vulkan/vulkan_core.h>

enum class HeadlessAov
{
  Color,
  DlssRRRawColor,
  PrimId,
  InstanceId,
  DlssRRDiffuseAlbedo,
  DlssRRSpecularAlbedo,
  DlssRRNormalRoughness,
  DlssRRMotionVector,
  Depth,
  DlssRRLinearDepth,
  DlssRRSpecularHitDistance,
  DistanceToCamera,
  LidarPointCloud,
};

struct HeadlessAovTexture
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
