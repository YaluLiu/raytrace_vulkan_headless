#include <sstream>
#include <glm/glm.hpp>
#include "hello_vulkan.hpp"
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

extern std::vector<std::string> defaultSearchPaths;

void HelloVulkan::updateInstance(uint32_t instanceId, glm::mat4 transform, bool visible)
{
  if(instanceId >= m_instances.size())
  {
    return;
  }
  m_instances[instanceId].transform = transform;
  m_instances[instanceId].visible   = visible;
}

void HelloVulkan::updateMeshGeometry(uint32_t meshId)
{
  std::vector<VertexObj>& now_vertices = m_Loader[meshId].m_vertices;
  ObjModel&               model        = m_objModel[meshId];

  nvvk::CommandPool genCmdBuf(m_device, m_graphicsQueueIndex);
  VkCommandBuffer   cmdBuf = genCmdBuf.createCommandBuffer();

  model.nbVertices = static_cast<uint32_t>(now_vertices.size());

  VkBufferUsageFlags storageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

  m_alloc.destroy(model.vertexBuffer);
  model.vertexBuffer = m_alloc.createBuffer(cmdBuf, now_vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | storageFlags);
  genCmdBuf.submitAndWait(cmdBuf);
}
