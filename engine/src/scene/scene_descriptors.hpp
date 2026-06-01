#pragma once

#include "nvvk/descriptorsets_vk.hpp"
#include "nvvk/resourceallocator_vk.hpp"
#include "shaders/common/host_device.h"

#include <span>
#include <vector>

#include <vulkan/vulkan_core.h>

class SceneDescriptors final
{
public:
  void create(VkDevice device, uint32_t textureDescriptorCount, bool includeTileFrameUniformBinding = true);
  void destroy();

  void update(const VkDescriptorBufferInfo& frameUniforms,
              const VkDescriptorBufferInfo& objectDescriptions,
              const VkDescriptorBufferInfo& lights,
              const VkDescriptorBufferInfo& tileFrameUniforms,
              std::span<const nvvk::Texture> textures);
  void updateFrameUniform(const VkDescriptorBufferInfo& frameUniforms);
  void updateTileFrameUniform(const VkDescriptorBufferInfo& tileFrameUniforms);

  VkDescriptorSetLayout layout() const { return m_descSetLayout; }
  VkDescriptorSet set() const { return m_descSet; }

private:
  nvvk::DescriptorSetBindings m_bindings;
  VkDevice m_device{VK_NULL_HANDLE};
  VkDescriptorPool m_descPool{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_descSetLayout{VK_NULL_HANDLE};
  VkDescriptorSet m_descSet{VK_NULL_HANDLE};
};
