#include "scene/mesh_store.hpp"

#include "nvvk/buffers_vk.hpp"
#include "nvvk/commands_vk.hpp"

#include <algorithm>
#include <string>

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

Material PrepareUploadMaterialForGpu(Material material)
{
  material.ambient = glm::pow(material.ambient, glm::vec3(2.2f));
  material.diffuse = glm::pow(material.diffuse, glm::vec3(2.2f));
  material.specular = glm::pow(material.specular, glm::vec3(2.2f));
  material.emission = glm::pow(material.emission, glm::vec3(2.2f));
  material.baseColorFactor = glm::pow(material.baseColorFactor, glm::vec3(2.2f));
  material.emissionFactor = glm::pow(material.emissionFactor, glm::vec3(2.2f));
  return material;
}
} // namespace

void MeshStore::setup(VkDevice device,
                      uint32_t graphicsQueueIndex,
                      nvvk::ResourceAllocatorDma& allocator,
                      nvvk::DebugUtil& debug)
{
  m_device = device;
  m_graphicsQueueIndex = graphicsQueueIndex;
  m_alloc = &allocator;
  m_debug = &debug;
}

void MeshStore::destroy()
{
  if(m_alloc != nullptr)
  {
    m_alloc->destroy(m_objectDescriptionBuffer);
  }
  m_objectDescriptionBuffer = {};
  destroyMeshBuffers();
  m_records.clear();
  m_objectDescriptions.clear();
}

uint32_t MeshStore::uploadMesh(const VkCommandBuffer& cmdBuf,
                               const MeshGeometry& geometry,
                               std::span<const Material> materials)
{
  std::vector<Material> gpuMaterials;
  gpuMaterials.reserve(materials.size());
  for(const Material& material : materials)
  {
    gpuMaterials.emplace_back(PrepareUploadMaterialForGpu(material));
  }

  MeshBuffers mesh;
  mesh.nbIndices = static_cast<uint32_t>(geometry.indices.size());
  mesh.nbVertices = static_cast<uint32_t>(geometry.vertices.size());
  mesh.vertexBufferSize = sizeof(MeshVertex) * geometry.vertices.size();
  mesh.indexBufferSize = sizeof(uint32_t) * geometry.indices.size();

  VkBufferUsageFlags deviceAddressFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
  mesh.vertexBuffer =
      m_alloc->createBuffer(cmdBuf, geometry.vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | deviceAddressFlags);
  mesh.indexBuffer =
      m_alloc->createBuffer(cmdBuf, geometry.indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | deviceAddressFlags);
  mesh.matColorBuffer = m_alloc->createBuffer(cmdBuf, gpuMaterials, deviceAddressFlags);
  mesh.matIndexBuffer = m_alloc->createBuffer(cmdBuf, geometry.materialIndices, deviceAddressFlags);

  std::string meshDebugIndex = std::to_string(m_records.size());
  m_debug->setObjectName(mesh.vertexBuffer.buffer, (std::string("vertex_" + meshDebugIndex)));
  m_debug->setObjectName(mesh.indexBuffer.buffer, (std::string("index_" + meshDebugIndex)));
  m_debug->setObjectName(mesh.matColorBuffer.buffer, (std::string("mat_" + meshDebugIndex)));
  m_debug->setObjectName(mesh.matIndexBuffer.buffer, (std::string("matIdx_" + meshDebugIndex)));

  ObjDesc desc;
  desc.txtOffset = 0;
  desc.vertexAddress = nvvk::getBufferDeviceAddress(m_device, mesh.vertexBuffer.buffer);
  desc.indexAddress = nvvk::getBufferDeviceAddress(m_device, mesh.indexBuffer.buffer);
  desc.materialAddress = nvvk::getBufferDeviceAddress(m_device, mesh.matColorBuffer.buffer);
  desc.materialIndexAddress = nvvk::getBufferDeviceAddress(m_device, mesh.matIndexBuffer.buffer);

  const uint32_t meshIndex = static_cast<uint32_t>(m_records.size());
  m_records.emplace_back(MeshRecord{geometry, mesh, desc});
  m_buffers.emplace_back(mesh);
  m_objectDescriptions.emplace_back(desc);
  return meshIndex;
}

bool MeshStore::updateGeometry(uint32_t meshIndex, const MeshGeometry& geometry)
{
  if(meshIndex >= m_records.size() || meshIndex >= m_buffers.size())
  {
    return false;
  }

  MeshRecord& record = m_records[meshIndex];
  record.geometry = geometry;

  std::vector<MeshVertex>& now_vertices = record.geometry.vertices;
  std::vector<uint32_t>& now_indices = record.geometry.indices;
  MeshBuffers& mesh = m_buffers[meshIndex];

  nvvk::CommandPool genCmdBuf(m_device, m_graphicsQueueIndex);
  VkCommandBuffer cmdBuf = genCmdBuf.createCommandBuffer();

  VkBufferUsageFlags storageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

  const uint32_t newNbVertices = static_cast<uint32_t>(now_vertices.size());
  const uint32_t newNbIndices = static_cast<uint32_t>(now_indices.size());
  const VkDeviceSize vertexBytes = sizeof(MeshVertex) * now_vertices.size();
  const VkDeviceSize indexBytes = sizeof(uint32_t) * now_indices.size();

  if(vertexBytes > 0)
  {
    if(mesh.vertexBuffer.buffer != VK_NULL_HANDLE && mesh.vertexBufferSize >= vertexBytes)
    {
      m_alloc->getStaging()->cmdToBuffer(cmdBuf, mesh.vertexBuffer.buffer, 0, vertexBytes, now_vertices.data());
      AddTransferBarrier(cmdBuf, mesh.vertexBuffer.buffer, vertexBytes, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
    }
    else
    {
      m_alloc->destroy(mesh.vertexBuffer);
      mesh.vertexBuffer =
          m_alloc->createBuffer(cmdBuf, vertexBytes, now_vertices.data(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | storageFlags);
      mesh.vertexBufferSize = vertexBytes;
      AddTransferBarrier(cmdBuf, mesh.vertexBuffer.buffer, vertexBytes, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);

      if(meshIndex < m_objectDescriptions.size())
      {
        m_objectDescriptions[meshIndex].vertexAddress = nvvk::getBufferDeviceAddress(m_device, mesh.vertexBuffer.buffer);
        record.objectDescription = m_objectDescriptions[meshIndex];
        if(m_objectDescriptionBuffer.buffer != VK_NULL_HANDLE)
        {
          const VkDeviceSize descOffset = sizeof(ObjDesc) * meshIndex;
          vkCmdUpdateBuffer(cmdBuf,
                            m_objectDescriptionBuffer.buffer,
                            descOffset,
                            sizeof(ObjDesc),
                            &m_objectDescriptions[meshIndex]);

          VkBufferMemoryBarrier descBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
          descBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
          descBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
          descBarrier.buffer = m_objectDescriptionBuffer.buffer;
          descBarrier.offset = descOffset;
          descBarrier.size = sizeof(ObjDesc);
          vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0,
                               nullptr, 1, &descBarrier, 0, nullptr);
        }
      }
    }
  }

  const bool indexCountChanged = mesh.nbIndices != newNbIndices;
  if(indexBytes > 0 && indexCountChanged)
  {
    if(mesh.indexBuffer.buffer != VK_NULL_HANDLE && mesh.indexBufferSize >= indexBytes)
    {
      m_alloc->getStaging()->cmdToBuffer(cmdBuf, mesh.indexBuffer.buffer, 0, indexBytes, now_indices.data());
    }
    else
    {
      m_alloc->destroy(mesh.indexBuffer);
      mesh.indexBuffer =
          m_alloc->createBuffer(cmdBuf, indexBytes, now_indices.data(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | storageFlags);
      mesh.indexBufferSize = indexBytes;
    }
    AddTransferBarrier(cmdBuf, mesh.indexBuffer.buffer, indexBytes, VK_ACCESS_INDEX_READ_BIT);
  }

  mesh.nbVertices = newNbVertices;
  mesh.nbIndices = newNbIndices;
  record.buffers = mesh;

  genCmdBuf.submitAndWait(cmdBuf);
  m_alloc->finalizeAndReleaseStaging();
  return true;
}

void MeshStore::updateMaterialsAtRuntime(const std::vector<MaterialUpdate>& updates)
{
  nvvk::CommandPool cmdGen(m_device, m_graphicsQueueIndex);
  VkCommandBuffer cmdBuf = cmdGen.createCommandBuffer();

  std::vector<VkBufferMemoryBarrier> preBarriers;
  std::vector<VkBufferMemoryBarrier> postBarriers;

  for(const auto& upd : updates)
  {
    VkDeviceSize offset = upd.materialIndex * sizeof(Material);

    VkBufferMemoryBarrier preBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    preBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    preBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    preBarrier.buffer = m_buffers[upd.meshIndex].matColorBuffer.buffer;
    preBarrier.offset = offset;
    preBarrier.size = sizeof(Material);

    preBarriers.push_back(preBarrier);
  }
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                       static_cast<uint32_t>(preBarriers.size()), preBarriers.data(), 0, nullptr);

  for(const auto& upd : updates)
  {
    VkDeviceSize offset = upd.materialIndex * sizeof(Material);
    vkCmdUpdateBuffer(cmdBuf, m_buffers[upd.meshIndex].matColorBuffer.buffer, offset, sizeof(Material),
                      &upd.newMaterial);
  }

  for(const auto& upd : updates)
  {
    VkDeviceSize offset = upd.materialIndex * sizeof(Material);

    VkBufferMemoryBarrier postBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    postBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    postBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    postBarrier.buffer = m_buffers[upd.meshIndex].matColorBuffer.buffer;
    postBarrier.offset = offset;
    postBarrier.size = sizeof(Material);

    postBarriers.push_back(postBarrier);
  }
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                       static_cast<uint32_t>(postBarriers.size()), postBarriers.data(), 0, nullptr);

  cmdGen.submitAndWait(cmdBuf);
}

void MeshStore::createObjectDescriptionBuffer()
{
  nvvk::CommandPool cmdGen(m_device, m_graphicsQueueIndex);

  auto cmdBuf = cmdGen.createCommandBuffer();
  const std::vector<ObjDesc> dummyObjDesc(1);
  const std::vector<ObjDesc>& objDesc = m_objectDescriptions.empty() ? dummyObjDesc : m_objectDescriptions;
  m_alloc->destroy(m_objectDescriptionBuffer);
  m_objectDescriptionBuffer = m_alloc->createBuffer(cmdBuf, objDesc, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  cmdGen.submitAndWait(cmdBuf);
  m_alloc->finalizeAndReleaseStaging();
  m_debug->setObjectName(m_objectDescriptionBuffer.buffer, "ObjDescs");
}

void MeshStore::destroyMeshBuffers()
{
  if(m_alloc != nullptr)
  {
    for(auto& mesh : m_buffers)
    {
      m_alloc->destroy(mesh.vertexBuffer);
      m_alloc->destroy(mesh.indexBuffer);
      m_alloc->destroy(mesh.matColorBuffer);
      m_alloc->destroy(mesh.matIndexBuffer);
    }
  }
  m_buffers.clear();
}
