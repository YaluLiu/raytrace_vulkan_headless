#include "hello_vulkan.hpp"

#include "nvvk/descriptorsets_vk.hpp"

extern std::vector<std::string> defaultSearchPaths;

void HelloVulkan::createRasterPipeline()
{
  m_mainRasterPipeline.createGraphicsPipeline(m_descSetLayout);
  m_tileRasterPipeline.createGraphicsPipeline(m_descSetLayout);
  m_tileMultiviewRasterPipeline.destroyGraphicsPipeline();
}

void HelloVulkan::rasterize(const VkCommandBuffer& cmdBuf)
{
  m_mainRasterPipeline.rasterize(cmdBuf, m_descSet, m_objModel, m_instances, m_instanceIds);
}

void HelloVulkan::createDescriptorSetLayout()
{
  m_descSetLayoutBind.clear();
  auto nbTxt = static_cast<uint32_t>(m_textures.size());

  m_descSetLayoutBind.addBinding(SceneBindings::eFrameUniforms, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1,
                                 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  m_descSetLayoutBind.addBinding(SceneBindings::eObjDescs, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
  m_descSetLayoutBind.addBinding(SceneBindings::eTextures, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nbTxt,
                                 VK_SHADER_STAGE_FRAGMENT_BIT);
  m_descSetLayoutBind.addBinding(SceneBindings::eLights, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                 VK_SHADER_STAGE_FRAGMENT_BIT);
  m_descSetLayoutBind.addBinding(SceneBindings::eTileFrameUniforms, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);

  m_descSetLayout = m_descSetLayoutBind.createLayout(m_device);
  m_descPool      = m_descSetLayoutBind.createPool(m_device, 1);
  m_descSet       = nvvk::allocateDescriptorSet(m_device, m_descPool, m_descSetLayout);
}

void HelloVulkan::updateDescriptorSet()
{
  std::vector<VkWriteDescriptorSet> writes;

  VkDescriptorBufferInfo dbiUnif{m_bFrameUniforms.buffer, 0, sizeof(FrameUniforms)};
  writes.emplace_back(m_descSetLayoutBind.makeWrite(m_descSet, SceneBindings::eFrameUniforms, &dbiUnif));

  VkDescriptorBufferInfo dbiSceneDesc{m_bObjDesc.buffer, 0, VK_WHOLE_SIZE};
  writes.emplace_back(m_descSetLayoutBind.makeWrite(m_descSet, SceneBindings::eObjDescs, &dbiSceneDesc));

  VkDescriptorBufferInfo dbiLights{m_bLights.buffer, 0, VK_WHOLE_SIZE};
  writes.emplace_back(m_descSetLayoutBind.makeWrite(m_descSet, SceneBindings::eLights, &dbiLights));

  VkDescriptorBufferInfo dbiTileUnif{m_bTileFrameUniforms.buffer, 0, sizeof(TileFrameUniforms)};
  writes.emplace_back(m_descSetLayoutBind.makeWrite(m_descSet, SceneBindings::eTileFrameUniforms, &dbiTileUnif));

  std::vector<VkDescriptorImageInfo> diit;
  for(auto& texture : m_textures)
  {
    diit.emplace_back(texture.descriptor);
  }
  writes.emplace_back(m_descSetLayoutBind.makeWriteArray(m_descSet, SceneBindings::eTextures, diit.data()));

  vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}
