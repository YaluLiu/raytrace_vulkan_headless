#include "scene/scene_descriptors.hpp"

void SceneDescriptors::create(VkDevice device, uint32_t textureDescriptorCount, bool includeTileFrameUniformBinding)
{
  destroy();

  m_device = device;
  m_bindings.clear();
  m_bindings.addBinding(SceneBindings::eFrameUniforms, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  m_bindings.addBinding(SceneBindings::eObjDescs, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  m_bindings.addBinding(SceneBindings::eTextures, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, textureDescriptorCount,
                        VK_SHADER_STAGE_FRAGMENT_BIT);
  m_bindings.addBinding(SceneBindings::eLights, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_FRAGMENT_BIT);
  if(includeTileFrameUniformBinding)
  {
    m_bindings.addBinding(SceneBindings::eTileFrameUniforms, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                          VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  }

  m_descSetLayout = m_bindings.createLayout(m_device);
  m_descPool      = m_bindings.createPool(m_device, 1);
  m_descSet       = nvvk::allocateDescriptorSet(m_device, m_descPool, m_descSetLayout);
}

void SceneDescriptors::destroy()
{
  if(m_device != VK_NULL_HANDLE)
  {
    vkDestroyDescriptorPool(m_device, m_descPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descSetLayout, nullptr);
  }

  m_descPool      = VK_NULL_HANDLE;
  m_descSetLayout = VK_NULL_HANDLE;
  m_descSet       = VK_NULL_HANDLE;
}

void SceneDescriptors::update(const VkDescriptorBufferInfo& frameUniforms,
                                    const VkDescriptorBufferInfo& objectDescriptions,
                                    const VkDescriptorBufferInfo& lights,
                                    const VkDescriptorBufferInfo& tileFrameUniforms,
                                    std::span<const nvvk::Texture> textures)
{
  if(m_device == VK_NULL_HANDLE || m_descSet == VK_NULL_HANDLE)
  {
    return;
  }

  std::vector<VkWriteDescriptorSet> writes;
  writes.emplace_back(m_bindings.makeWrite(m_descSet, SceneBindings::eFrameUniforms, &frameUniforms));
  writes.emplace_back(m_bindings.makeWrite(m_descSet, SceneBindings::eObjDescs, &objectDescriptions));
  writes.emplace_back(m_bindings.makeWrite(m_descSet, SceneBindings::eLights, &lights));
  writes.emplace_back(m_bindings.makeWrite(m_descSet, SceneBindings::eTileFrameUniforms, &tileFrameUniforms));

  std::vector<VkDescriptorImageInfo> imageInfos;
  imageInfos.reserve(textures.size());
  for(const nvvk::Texture& texture : textures)
  {
    imageInfos.emplace_back(texture.descriptor);
  }
  writes.emplace_back(m_bindings.makeWriteArray(m_descSet, SceneBindings::eTextures, imageInfos.data()));

  vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

void SceneDescriptors::updateFrameUniform(const VkDescriptorBufferInfo& frameUniforms)
{
  if(m_device == VK_NULL_HANDLE || m_descSet == VK_NULL_HANDLE || m_descSetLayout == VK_NULL_HANDLE ||
     frameUniforms.buffer == VK_NULL_HANDLE)
  {
    return;
  }

  VkWriteDescriptorSet write = m_bindings.makeWrite(m_descSet, SceneBindings::eFrameUniforms, &frameUniforms);
  vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

void SceneDescriptors::updateTileFrameUniform(const VkDescriptorBufferInfo& tileFrameUniforms)
{
  if(m_device == VK_NULL_HANDLE || m_descSet == VK_NULL_HANDLE || m_descSetLayout == VK_NULL_HANDLE ||
     tileFrameUniforms.buffer == VK_NULL_HANDLE)
  {
    return;
  }

  VkWriteDescriptorSet write = m_bindings.makeWrite(m_descSet, SceneBindings::eTileFrameUniforms, &tileFrameUniforms);
  vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}
