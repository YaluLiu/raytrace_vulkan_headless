#include <sstream>
#include <glm/glm.hpp>
#include "raster_renderer.hpp"
#include "nvh/alignment.hpp"
#include "nvh/cameramanipulator.hpp"
#include "nvh/fileoperations.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/descriptorsets_vk.hpp"
#include "nvvk/images_vk.hpp"
#include "nvvk/pipeline_vk.hpp"
#include "nvvk/renderpasses_vk.hpp"
#include "nvvk/shaders_vk.hpp"
#include "nvvk/buffers_vk.hpp"
#include "nvvk/stagingmemorymanager_vk.hpp"

extern std::vector<std::string> defaultSearchPaths;

namespace
{
void AddTransferBarrier(VkCommandBuffer cmdBuf, VkBuffer buffer, VkDeviceSize size, VkAccessFlags dstAccessMask)
{
  if(buffer == VK_NULL_HANDLE || size == 0)
  {
    return;
  }

  VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = dstAccessMask;
  barrier.buffer = buffer;
  barrier.offset = 0;
  barrier.size = size;
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 1,
                       &barrier, 0, nullptr);
}
} // namespace

void RasterRenderer::updateInstance(uint32_t instanceId, glm::mat4 transform, bool visible)
{
  if(instanceId >= m_instances.size())
  {
    return;
  }
  m_instances[instanceId].transform = transform;
  m_instances[instanceId].visible   = visible;
}

void RasterRenderer::updateMeshGeometry(uint32_t meshId)
{
  if(meshId >= m_Loader.size() || meshId >= m_objModel.size())
  {
    return;
  }

  std::vector<VertexObj>& now_vertices = m_Loader[meshId].m_vertices;
  std::vector<uint32_t>& now_indices = m_Loader[meshId].m_indices;
  ObjModel&               model        = m_objModel[meshId];

  nvvk::CommandPool genCmdBuf(m_device, m_graphicsQueueIndex);
  VkCommandBuffer   cmdBuf = genCmdBuf.createCommandBuffer();

  VkBufferUsageFlags storageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

  const uint32_t newNbVertices = static_cast<uint32_t>(now_vertices.size());
  const uint32_t newNbIndices = static_cast<uint32_t>(now_indices.size());
  const VkDeviceSize vertexBytes = sizeof(VertexObj) * now_vertices.size();
  const VkDeviceSize indexBytes = sizeof(uint32_t) * now_indices.size();

  if(vertexBytes > 0)
  {
    if(model.vertexBuffer.buffer != VK_NULL_HANDLE && model.vertexBufferSize >= vertexBytes)
    {
      m_alloc.getStaging()->cmdToBuffer(cmdBuf, model.vertexBuffer.buffer, 0, vertexBytes, now_vertices.data());
      AddTransferBarrier(cmdBuf, model.vertexBuffer.buffer, vertexBytes, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
    }
    else
    {
      m_alloc.destroy(model.vertexBuffer);
      model.vertexBuffer = m_alloc.createBuffer(cmdBuf, vertexBytes, now_vertices.data(),
                                                VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | storageFlags);
      model.vertexBufferSize = vertexBytes;
      AddTransferBarrier(cmdBuf, model.vertexBuffer.buffer, vertexBytes, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);

      if(meshId < m_objDesc.size())
      {
        m_objDesc[meshId].vertexAddress = nvvk::getBufferDeviceAddress(m_device, model.vertexBuffer.buffer);
        if(m_bObjDesc.buffer != VK_NULL_HANDLE)
        {
          const VkDeviceSize descOffset = sizeof(ObjDesc) * meshId;
          vkCmdUpdateBuffer(cmdBuf, m_bObjDesc.buffer, descOffset, sizeof(ObjDesc), &m_objDesc[meshId]);

          VkBufferMemoryBarrier descBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
          descBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
          descBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
          descBarrier.buffer = m_bObjDesc.buffer;
          descBarrier.offset = descOffset;
          descBarrier.size = sizeof(ObjDesc);
          vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                               nullptr, 1, &descBarrier, 0, nullptr);
        }
      }
    }
  }

  const bool indexCountChanged = model.nbIndices != newNbIndices;
  if(indexBytes > 0 && indexCountChanged)
  {
    if(model.indexBuffer.buffer != VK_NULL_HANDLE && model.indexBufferSize >= indexBytes)
    {
      m_alloc.getStaging()->cmdToBuffer(cmdBuf, model.indexBuffer.buffer, 0, indexBytes, now_indices.data());
    }
    else
    {
      m_alloc.destroy(model.indexBuffer);
      model.indexBuffer =
          m_alloc.createBuffer(cmdBuf, indexBytes, now_indices.data(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | storageFlags);
      model.indexBufferSize = indexBytes;
    }
    AddTransferBarrier(cmdBuf, model.indexBuffer.buffer, indexBytes, VK_ACCESS_INDEX_READ_BIT);
  }

  model.nbVertices = newNbVertices;
  model.nbIndices = newNbIndices;

  genCmdBuf.submitAndWait(cmdBuf);
  m_alloc.finalizeAndReleaseStaging();
}
