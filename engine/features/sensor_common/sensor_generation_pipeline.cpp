#include "features/sensor_common/sensor_generation_pipeline.hpp"

#include "nvh/fileoperations.hpp"
#include "nvvk/shaders_vk.hpp"

#include <stdexcept>
#include <string>
#include <vector>

extern std::vector<std::string> defaultSearchPaths;

void SensorGenerationPipeline::setup(VkDevice device,
                                     nvvk::DebugUtil& debug,
                                     SensorGenerationPipelineConfig config)
{
  m_device = device;
  m_debug = &debug;
  m_debugLabel = config.debugLabel != nullptr ? config.debugLabel : "SensorGenerate";
  m_shaderSpvPath = config.shaderSpvPath != nullptr ? config.shaderSpvPath : "";
  m_pushConstantSize = config.pushConstantSize;
}

void SensorGenerationPipeline::destroy()
{
  if(m_device != VK_NULL_HANDLE)
  {
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    vkDestroyDescriptorPool(m_device, m_descriptorPool, nullptr);
    vkDestroyDescriptorSetLayout(m_device, m_descriptorSetLayout, nullptr);
  }
  m_pipeline = VK_NULL_HANDLE;
  m_pipelineLayout = VK_NULL_HANDLE;
  m_descriptorPool = VK_NULL_HANDLE;
  m_descriptorSetLayout = VK_NULL_HANDLE;
  m_descriptorSet = VK_NULL_HANDLE;
  m_boundTlas = VK_NULL_HANDLE;
  m_boundSensorMetadataBuffer = VK_NULL_HANDLE;
  m_boundGeneratedSamplesBuffer = VK_NULL_HANDLE;
  m_device = VK_NULL_HANDLE;
  m_debug = nullptr;
  m_debugLabel.clear();
  m_shaderSpvPath.clear();
  m_pushConstantSize = 0;
}

bool SensorGenerationPipeline::ensureResources(const TlasDescriptorInfo& tlasInfo,
                                               VkBuffer sensorMetadataBuffer,
                                               VkBuffer generatedSamplesBuffer)
{
  if(m_device == VK_NULL_HANDLE || tlasInfo.accelerationStructure == VK_NULL_HANDLE ||
     sensorMetadataBuffer == VK_NULL_HANDLE || generatedSamplesBuffer == VK_NULL_HANDLE ||
     m_shaderSpvPath.empty())
  {
    return false;
  }
  if(m_descriptorSetLayout == VK_NULL_HANDLE)
  {
    createDescriptorResources();
  }
  if(m_pipeline == VK_NULL_HANDLE)
  {
    createPipeline();
  }
  if(m_boundTlas != tlasInfo.accelerationStructure ||
     m_boundSensorMetadataBuffer != sensorMetadataBuffer ||
     m_boundGeneratedSamplesBuffer != generatedSamplesBuffer)
  {
    updateDescriptorSet(tlasInfo, sensorMetadataBuffer, generatedSamplesBuffer);
  }
  return m_descriptorSet != VK_NULL_HANDLE && m_pipeline != VK_NULL_HANDLE && m_pipelineLayout != VK_NULL_HANDLE;
}

void SensorGenerationPipeline::dispatch(const VkCommandBuffer& cmdBuf,
                                        const void* pushConstant,
                                        uint32_t width,
                                        uint32_t height)
{
  if(m_pipeline == VK_NULL_HANDLE || m_pipelineLayout == VK_NULL_HANDLE || m_descriptorSet == VK_NULL_HANDLE ||
     width == 0 || height == 0)
  {
    return;
  }

  if(m_debug != nullptr && !m_debugLabel.empty())
  {
    m_debug->beginLabel(cmdBuf, m_debugLabel.c_str());
  }
  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipeline);
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_COMPUTE, m_pipelineLayout, 0, 1, &m_descriptorSet, 0,
                          nullptr);

  if(pushConstant != nullptr && m_pushConstantSize > 0)
  {
    vkCmdPushConstants(cmdBuf, m_pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       static_cast<uint32_t>(m_pushConstantSize), pushConstant);
  }

  const uint32_t groupCountX = (width + 7u) / 8u;
  const uint32_t groupCountY = (height + 7u) / 8u;
  vkCmdDispatch(cmdBuf, groupCountX, groupCountY, 1);
  if(m_debug != nullptr && !m_debugLabel.empty())
  {
    m_debug->endLabel(cmdBuf);
  }
}

void SensorGenerationPipeline::createDescriptorResources()
{
  m_bindings.clear();
  m_bindings.addBinding(0, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1, VK_SHADER_STAGE_COMPUTE_BIT);
  m_bindings.addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
  m_bindings.addBinding(2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT);
  m_descriptorSetLayout = m_bindings.createLayout(m_device);
  m_descriptorPool = m_bindings.createPool(m_device, 1);
  m_descriptorSet = nvvk::allocateDescriptorSet(m_device, m_descriptorPool, m_descriptorSetLayout);
}

void SensorGenerationPipeline::createPipeline()
{
  vkDestroyPipeline(m_device, m_pipeline, nullptr);
  vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);

  VkPushConstantRange pushConstant{VK_SHADER_STAGE_COMPUTE_BIT, 0,
                                   static_cast<uint32_t>(m_pushConstantSize)};
  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutCreateInfo.setLayoutCount = 1;
  pipelineLayoutCreateInfo.pSetLayouts = &m_descriptorSetLayout;
  pipelineLayoutCreateInfo.pushConstantRangeCount = m_pushConstantSize > 0 ? 1u : 0u;
  pipelineLayoutCreateInfo.pPushConstantRanges = m_pushConstantSize > 0 ? &pushConstant : nullptr;
  if(vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS)
  {
    throw std::runtime_error("Failed to create " + m_debugLabel + " pipeline layout");
  }

  VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
  pipelineInfo.layout = m_pipelineLayout;
  pipelineInfo.stage = nvvk::createShaderStageInfo(
      m_device, nvh::loadFile(m_shaderSpvPath, true, defaultSearchPaths, true), VK_SHADER_STAGE_COMPUTE_BIT);
  if(vkCreateComputePipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline) != VK_SUCCESS)
  {
    vkDestroyShaderModule(m_device, pipelineInfo.stage.module, nullptr);
    throw std::runtime_error("Failed to create " + m_debugLabel + " compute pipeline");
  }
  vkDestroyShaderModule(m_device, pipelineInfo.stage.module, nullptr);
}

void SensorGenerationPipeline::updateDescriptorSet(const TlasDescriptorInfo& tlasInfo,
                                                   VkBuffer sensorMetadataBuffer,
                                                   VkBuffer generatedSamplesBuffer)
{
  VkWriteDescriptorSetAccelerationStructureKHR accelerationStructure{
      VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
  accelerationStructure.accelerationStructureCount = 1;
  accelerationStructure.pAccelerationStructures = &tlasInfo.accelerationStructure;

  VkDescriptorBufferInfo sensorMetadataInfo{sensorMetadataBuffer, 0, VK_WHOLE_SIZE};
  VkDescriptorBufferInfo generatedSamplesInfo{generatedSamplesBuffer, 0, VK_WHOLE_SIZE};

  std::vector<VkWriteDescriptorSet> writes;
  VkWriteDescriptorSet tlasWrite{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
  tlasWrite.pNext = &accelerationStructure;
  tlasWrite.dstSet = m_descriptorSet;
  tlasWrite.dstBinding = 0;
  tlasWrite.descriptorCount = 1;
  tlasWrite.descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
  writes.emplace_back(tlasWrite);
  writes.emplace_back(m_bindings.makeWrite(m_descriptorSet, 1, &sensorMetadataInfo));
  writes.emplace_back(m_bindings.makeWrite(m_descriptorSet, 2, &generatedSamplesInfo));
  vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);

  m_boundTlas = tlasInfo.accelerationStructure;
  m_boundSensorMetadataBuffer = sensorMetadataBuffer;
  m_boundGeneratedSamplesBuffer = generatedSamplesBuffer;
}
