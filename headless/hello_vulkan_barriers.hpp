#pragma once

#include <vulkan/vulkan_core.h>

inline VkImageMemoryBarrier makeColorImageBarrier(VkImage image,
                                                  VkAccessFlags srcAccessMask,
                                                  VkAccessFlags dstAccessMask,
                                                  VkImageLayout oldLayout,
                                                  VkImageLayout newLayout)
{
  VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
  barrier.srcAccessMask    = srcAccessMask;
  barrier.dstAccessMask    = dstAccessMask;
  barrier.oldLayout        = oldLayout;
  barrier.newLayout        = newLayout;
  barrier.image            = image;
  barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
  return barrier;
}

inline VkImageMemoryBarrier makeGeneralColorImageBarrier(VkImage image,
                                                         VkAccessFlags srcAccessMask,
                                                         VkAccessFlags dstAccessMask)
{
  return makeColorImageBarrier(image, srcAccessMask, dstAccessMask, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
}

inline void insertGeneralImageBarrier(VkCommandBuffer      cmdBuf,
                                      VkImage              image,
                                      VkAccessFlags        srcAccessMask,
                                      VkAccessFlags        dstAccessMask,
                                      VkPipelineStageFlags srcStageMask,
                                      VkPipelineStageFlags dstStageMask)
{
  const VkImageMemoryBarrier barrier = makeGeneralColorImageBarrier(image, srcAccessMask, dstAccessMask);
  vkCmdPipelineBarrier(cmdBuf, srcStageMask, dstStageMask, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}
