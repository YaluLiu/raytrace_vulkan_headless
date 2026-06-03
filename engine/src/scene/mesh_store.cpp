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

  MeshBuffers model;
  model.nbIndices = static_cast<uint32_t>(geometry.indices.size());
  model.nbVertices = static_cast<uint32_t>(geometry.vertices.size());
  model.vertexBufferSize = sizeof(MeshVertex) * geometry.vertices.size();
  model.indexBufferSize = sizeof(uint32_t) * geometry.indices.size();

  VkBufferUsageFlags deviceAddressFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
  model.vertexBuffer =
      m_alloc->createBuffer(cmdBuf, geometry.vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | deviceAddressFlags);
  model.indexBuffer =
      m_alloc->createBuffer(cmdBuf, geometry.indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | deviceAddressFlags);
  model.matColorBuffer = m_alloc->createBuffer(cmdBuf, gpuMaterials, deviceAddressFlags);
  model.matIndexBuffer = m_alloc->createBuffer(cmdBuf, geometry.materialIndices, deviceAddressFlags);

  std::string objNb = std::to_string(m_records.size());
  m_debug->setObjectName(model.vertexBuffer.buffer, (std::string("vertex_" + objNb)));
  m_debug->setObjectName(model.indexBuffer.buffer, (std::string("index_" + objNb)));
  m_debug->setObjectName(model.matColorBuffer.buffer, (std::string("mat_" + objNb)));
  m_debug->setObjectName(model.matIndexBuffer.buffer, (std::string("matIdx_" + objNb)));

  ObjDesc desc;
  desc.txtOffset = 0;
  desc.vertexAddress = nvvk::getBufferDeviceAddress(m_device, model.vertexBuffer.buffer);
  desc.indexAddress = nvvk::getBufferDeviceAddress(m_device, model.indexBuffer.buffer);
  desc.materialAddress = nvvk::getBufferDeviceAddress(m_device, model.matColorBuffer.buffer);
  desc.materialIndexAddress = nvvk::getBufferDeviceAddress(m_device, model.matIndexBuffer.buffer);

  const uint32_t meshIndex = static_cast<uint32_t>(m_records.size());
  m_records.emplace_back(MeshRecord{geometry, model, desc});
  m_buffers.emplace_back(model);
  m_objectDescriptions.emplace_back(desc);
  return meshIndex;
}

bool MeshStore::updateGeometry(uint32_t meshId, const MeshGeometry& geometry)
{
  if(meshId >= m_records.size() || meshId >= m_buffers.size())
  {
    return false;
  }

  MeshRecord& record = m_records[meshId];
  record.geometry = geometry;

  std::vector<MeshVertex>& now_vertices = record.geometry.vertices;
  std::vector<uint32_t>& now_indices = record.geometry.indices;
  MeshBuffers& model = m_buffers[meshId];

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
    if(model.vertexBuffer.buffer != VK_NULL_HANDLE && model.vertexBufferSize >= vertexBytes)
    {
      m_alloc->getStaging()->cmdToBuffer(cmdBuf, model.vertexBuffer.buffer, 0, vertexBytes, now_vertices.data());
      AddTransferBarrier(cmdBuf, model.vertexBuffer.buffer, vertexBytes, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);
    }
    else
    {
      m_alloc->destroy(model.vertexBuffer);
      model.vertexBuffer =
          m_alloc->createBuffer(cmdBuf, vertexBytes, now_vertices.data(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | storageFlags);
      model.vertexBufferSize = vertexBytes;
      AddTransferBarrier(cmdBuf, model.vertexBuffer.buffer, vertexBytes, VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT);

      if(meshId < m_objectDescriptions.size())
      {
        m_objectDescriptions[meshId].vertexAddress = nvvk::getBufferDeviceAddress(m_device, model.vertexBuffer.buffer);
        record.objectDescription = m_objectDescriptions[meshId];
        if(m_objectDescriptionBuffer.buffer != VK_NULL_HANDLE)
        {
          const VkDeviceSize descOffset = sizeof(ObjDesc) * meshId;
          vkCmdUpdateBuffer(cmdBuf,
                            m_objectDescriptionBuffer.buffer,
                            descOffset,
                            sizeof(ObjDesc),
                            &m_objectDescriptions[meshId]);

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

  const bool indexCountChanged = model.nbIndices != newNbIndices;
  if(indexBytes > 0 && indexCountChanged)
  {
    if(model.indexBuffer.buffer != VK_NULL_HANDLE && model.indexBufferSize >= indexBytes)
    {
      m_alloc->getStaging()->cmdToBuffer(cmdBuf, model.indexBuffer.buffer, 0, indexBytes, now_indices.data());
    }
    else
    {
      m_alloc->destroy(model.indexBuffer);
      model.indexBuffer =
          m_alloc->createBuffer(cmdBuf, indexBytes, now_indices.data(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | storageFlags);
      model.indexBufferSize = indexBytes;
    }
    AddTransferBarrier(cmdBuf, model.indexBuffer.buffer, indexBytes, VK_ACCESS_INDEX_READ_BIT);
  }

  model.nbVertices = newNbVertices;
  model.nbIndices = newNbIndices;
  record.buffers = model;

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
    preBarrier.buffer = m_buffers[upd.modelIndex].matColorBuffer.buffer;
    preBarrier.offset = offset;
    preBarrier.size = sizeof(Material);

    preBarriers.push_back(preBarrier);
  }
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                       static_cast<uint32_t>(preBarriers.size()), preBarriers.data(), 0, nullptr);

  for(const auto& upd : updates)
  {
    VkDeviceSize offset = upd.materialIndex * sizeof(Material);
    vkCmdUpdateBuffer(cmdBuf, m_buffers[upd.modelIndex].matColorBuffer.buffer, offset, sizeof(Material),
                      &upd.newMaterial);
  }

  for(const auto& upd : updates)
  {
    VkDeviceSize offset = upd.materialIndex * sizeof(Material);

    VkBufferMemoryBarrier postBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    postBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    postBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    postBarrier.buffer = m_buffers[upd.modelIndex].matColorBuffer.buffer;
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
    for(auto& model : m_buffers)
    {
      m_alloc->destroy(model.vertexBuffer);
      m_alloc->destroy(model.indexBuffer);
      m_alloc->destroy(model.matColorBuffer);
      m_alloc->destroy(model.matIndexBuffer);
    }
  }
  m_buffers.clear();
}
