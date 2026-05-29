#include "scene/raster_gpu_scene.hpp"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#include "nvvk/buffers_vk.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/images_vk.hpp"

static_assert(std::is_standard_layout_v<RasterMaterial>);
static_assert(std::is_standard_layout_v<WaveFrontMaterial>);
static_assert(sizeof(RasterMaterial) == sizeof(WaveFrontMaterial));
static_assert(sizeof(RasterVertex) == sizeof(Vertex));
static_assert(offsetof(RasterVertex, pos) == offsetof(Vertex, pos));
static_assert(offsetof(RasterVertex, tangent) == offsetof(Vertex, tangent));
static_assert(offsetof(RasterMaterial, baseColorFactor) == offsetof(WaveFrontMaterial, baseColorFactor));
static_assert(offsetof(RasterMaterial, transmissionColorFactor) == offsetof(WaveFrontMaterial, transmissionColorFactor));
static_assert(offsetof(RasterMaterial, roughnessFactor) == offsetof(WaveFrontMaterial, roughnessFactor));
static_assert(offsetof(RasterMaterial, transmissionFactor) == offsetof(WaveFrontMaterial, transmissionFactor));
static_assert(offsetof(RasterMaterial, subsurfaceFactor) == offsetof(WaveFrontMaterial, subsurfaceFactor));
static_assert(offsetof(RasterMaterial, diffuseTextureId) == offsetof(WaveFrontMaterial, diffuseTextureId));
static_assert(offsetof(RasterMaterial, normalTextureId) == offsetof(WaveFrontMaterial, normalTextureId));
static_assert(offsetof(RasterMaterial, subsurfaceTextureId) == offsetof(WaveFrontMaterial, subsurfaceTextureId));

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

RasterMaterial PrepareUploadMaterialForGpu(RasterMaterial material)
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

void RasterGpuScene::setup(VkDevice device,
                           uint32_t graphicsQueueIndex,
                           nvvk::ResourceAllocatorDma& allocator,
                           nvvk::DebugUtil& debug)
{
  m_device = device;
  m_graphicsQueueIndex = graphicsQueueIndex;
  m_alloc = &allocator;
  m_debug = &debug;
  m_rtScene.setup(m_device, m_graphicsQueueIndex, *m_alloc);
}

void RasterGpuScene::destroy()
{
  if(m_alloc != nullptr)
  {
    m_alloc->destroy(m_bObjDesc);
    m_alloc->destroy(m_bLights);
  }
  m_bObjDesc = {};
  m_bLights = {};
  destroyRayTracingResources();
  destroyMeshBuffers();
  destroyTextures();
  m_meshGeometries.clear();
  m_objDesc.clear();
  m_instances.clear();
  m_instanceIds.clear();
  m_lights.clear();
}

uint32_t RasterGpuScene::addInstance(const glm::mat4& transform, uint32_t objIndex, int instanceId)
{
  RasterInstance instance;
  instance.transform = transform;
  instance.objIndex = objIndex;
  m_instances.push_back(instance);
  m_instanceIds.push_back(instanceId);
  return static_cast<uint32_t>(m_instances.size() - 1);
}

void RasterGpuScene::uploadMesh(const RasterMeshGeometry& geometry,
                                std::span<const RasterMaterial> materials,
                                glm::mat4 transform)
{
  std::vector<RasterMaterial> gpuMaterials;
  gpuMaterials.reserve(materials.size());
  for(const RasterMaterial& material : materials)
  {
    gpuMaterials.emplace_back(PrepareUploadMaterialForGpu(material));
  }

  RasterMeshBuffers model;
  model.nbIndices = static_cast<uint32_t>(geometry.indices.size());
  model.nbVertices = static_cast<uint32_t>(geometry.vertices.size());
  model.vertexBufferSize = sizeof(RasterVertex) * geometry.vertices.size();
  model.indexBufferSize = sizeof(uint32_t) * geometry.indices.size();

  nvvk::CommandPool cmdBufGet(m_device, m_graphicsQueueIndex);
  VkCommandBuffer cmdBuf = cmdBufGet.createCommandBuffer();
  VkBufferUsageFlags deviceAddressFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;
  model.vertexBuffer =
      m_alloc->createBuffer(cmdBuf, geometry.vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | deviceAddressFlags);
  model.indexBuffer =
      m_alloc->createBuffer(cmdBuf, geometry.indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | deviceAddressFlags);
  model.matColorBuffer = m_alloc->createBuffer(cmdBuf, gpuMaterials, deviceAddressFlags);
  model.matIndexBuffer = m_alloc->createBuffer(cmdBuf, geometry.materialIndices, deviceAddressFlags);

  uploadTextureResources(cmdBuf, {});
  cmdBufGet.submitAndWait(cmdBuf);

  m_alloc->finalizeAndReleaseStaging();

  std::string objNb = std::to_string(m_objModel.size());
  m_debug->setObjectName(model.vertexBuffer.buffer, (std::string("vertex_" + objNb)));
  m_debug->setObjectName(model.indexBuffer.buffer, (std::string("index_" + objNb)));
  m_debug->setObjectName(model.matColorBuffer.buffer, (std::string("mat_" + objNb)));
  m_debug->setObjectName(model.matIndexBuffer.buffer, (std::string("matIdx_" + objNb)));

  addInstance(transform, static_cast<uint32_t>(m_objModel.size()), 0);

  ObjDesc desc;
  desc.txtOffset = 0;
  desc.vertexAddress = nvvk::getBufferDeviceAddress(m_device, model.vertexBuffer.buffer);
  desc.indexAddress = nvvk::getBufferDeviceAddress(m_device, model.indexBuffer.buffer);
  desc.materialAddress = nvvk::getBufferDeviceAddress(m_device, model.matColorBuffer.buffer);
  desc.materialIndexAddress = nvvk::getBufferDeviceAddress(m_device, model.matIndexBuffer.buffer);

  m_objModel.emplace_back(model);
  m_objDesc.emplace_back(desc);
  m_meshGeometries.emplace_back(geometry);
}

void RasterGpuScene::loadTextureAssets(const std::vector<TextureAsset>& textureAssets)
{
  nvvk::CommandPool cmdBufGet(m_device, m_graphicsQueueIndex);
  VkCommandBuffer cmdBuf = cmdBufGet.createCommandBuffer();
  uploadTextureResources(cmdBuf, textureAssets);
  cmdBufGet.submitAndWait(cmdBuf);
  m_alloc->finalizeAndReleaseStaging();
}

void RasterGpuScene::rebuildTextureResources(const std::vector<TextureAsset>& textureAssets)
{
  destroyTextures();
  loadTextureAssets(textureAssets);
}

void RasterGpuScene::uploadTextureResources(const VkCommandBuffer& cmdBuf,
                                            const std::vector<TextureAsset>& textureAssets)
{
  VkSamplerCreateInfo samplerCreateInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
  samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
  samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerCreateInfo.maxLod = FLT_MAX;

  auto formatForColorSpace = [](TextureColorSpace colorSpace)
  { return colorSpace == TextureColorSpace::Linear ? VK_FORMAT_R8G8B8A8_UNORM : VK_FORMAT_R8G8B8A8_SRGB; };

  auto uploadTexture = [&](const stbi_uc* pixels, int texWidth, int texHeight, TextureColorSpace colorSpace)
  {
    VkFormat format = formatForColorSpace(colorSpace);
    VkDeviceSize bufferSize = static_cast<uint64_t>(texWidth) * texHeight * sizeof(uint8_t) * 4;
    auto imgSize = VkExtent2D{static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight)};
    auto imageCreateInfo = nvvk::makeImage2DCreateInfo(imgSize, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);

    nvvk::Image image = m_alloc->createImage(cmdBuf, bufferSize, pixels, imageCreateInfo);
    nvvk::cmdGenerateMipmaps(cmdBuf, image.image, format, imgSize, imageCreateInfo.mipLevels);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, imageCreateInfo);
    nvvk::Texture texture = m_alloc->createTexture(image, ivInfo, samplerCreateInfo);

    m_textures.push_back(texture);
  };

  if(textureAssets.empty() && m_textures.empty())
  {
    nvvk::Texture texture;

    std::array<uint8_t, 4> color{255u, 255u, 255u, 255u};
    VkDeviceSize bufferSize = sizeof(color);
    auto imgSize = VkExtent2D{1, 1};
    VkFormat format = formatForColorSpace(TextureColorSpace::SRGB);
    auto imageCreateInfo = nvvk::makeImage2DCreateInfo(imgSize, format);

    nvvk::Image image = m_alloc->createImage(cmdBuf, bufferSize, color.data(), imageCreateInfo);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, imageCreateInfo);
    texture = m_alloc->createTexture(image, ivInfo, samplerCreateInfo);

    nvvk::cmdBarrierImageLayout(cmdBuf, texture.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_textures.push_back(texture);
  }
  else
  {
    for(const TextureAsset& textureAsset : textureAssets)
    {
      int texWidth = 0;
      int texHeight = 0;
      int texChannels = 0;
      stbi_uc* stbi_pixels = nullptr;
      if(!textureAsset.encodedBytes.empty() &&
         textureAsset.encodedBytes.size() <= static_cast<size_t>(std::numeric_limits<int>::max()))
      {
        stbi_pixels =
            stbi_load_from_memory(textureAsset.encodedBytes.data(), static_cast<int>(textureAsset.encodedBytes.size()),
                                  &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
      }

      std::array<stbi_uc, 4> color{255u, 0u, 255u, 255u};

      stbi_uc* pixels = stbi_pixels;
      if(!stbi_pixels)
      {
        texWidth = texHeight = 1;
        texChannels = 4;
        pixels = reinterpret_cast<stbi_uc*>(color.data());
      }

      uploadTexture(pixels, texWidth, texHeight, textureAsset.colorSpace);

      stbi_image_free(stbi_pixels);
    }
  }
}

void RasterGpuScene::updateInstance(uint32_t instanceId, glm::mat4 transform, bool visible, uint32_t traceMask)
{
  if(instanceId >= m_instances.size())
  {
    return;
  }
  m_instances[instanceId].transform = transform;
  m_instances[instanceId].visible = visible;
  m_instances[instanceId].traceMask = traceMask;
  m_rtScene.markTlasDirty();
}

void RasterGpuScene::updateMeshGeometry(uint32_t meshId, const RasterMeshGeometry& geometry)
{
  if(meshId >= m_meshGeometries.size() || meshId >= m_objModel.size())
  {
    return;
  }

  m_meshGeometries[meshId] = geometry;

  std::vector<RasterVertex>& now_vertices = m_meshGeometries[meshId].vertices;
  std::vector<uint32_t>& now_indices = m_meshGeometries[meshId].indices;
  RasterMeshBuffers& model = m_objModel[meshId];

  nvvk::CommandPool genCmdBuf(m_device, m_graphicsQueueIndex);
  VkCommandBuffer cmdBuf = genCmdBuf.createCommandBuffer();

  VkBufferUsageFlags storageFlags = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
                                    VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR;

  const uint32_t newNbVertices = static_cast<uint32_t>(now_vertices.size());
  const uint32_t newNbIndices = static_cast<uint32_t>(now_indices.size());
  const VkDeviceSize vertexBytes = sizeof(RasterVertex) * now_vertices.size();
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
  m_rtScene.markBlasDirty(meshId);

  genCmdBuf.submitAndWait(cmdBuf);
  m_alloc->finalizeAndReleaseStaging();
}

void RasterGpuScene::updateMaterialsAtRuntime(const std::vector<RasterMaterialUpdate>& updates)
{
  nvvk::CommandPool cmdGen(m_device, m_graphicsQueueIndex);
  VkCommandBuffer cmdBuf = cmdGen.createCommandBuffer();

  std::vector<VkBufferMemoryBarrier> preBarriers;
  std::vector<VkBufferMemoryBarrier> postBarriers;

  for(const auto& upd : updates)
  {
    VkDeviceSize offset = upd.materialIndex * sizeof(RasterMaterial);

    VkBufferMemoryBarrier preBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    preBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    preBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    preBarrier.buffer = m_objModel[upd.modelIndex].matColorBuffer.buffer;
    preBarrier.offset = offset;
    preBarrier.size = sizeof(RasterMaterial);

    preBarriers.push_back(preBarrier);
  }
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                       static_cast<uint32_t>(preBarriers.size()), preBarriers.data(), 0, nullptr);

  for(const auto& upd : updates)
  {
    VkDeviceSize offset = upd.materialIndex * sizeof(RasterMaterial);
    vkCmdUpdateBuffer(cmdBuf, m_objModel[upd.modelIndex].matColorBuffer.buffer, offset, sizeof(RasterMaterial),
                      &upd.newMaterial);
  }

  for(const auto& upd : updates)
  {
    VkDeviceSize offset = upd.materialIndex * sizeof(RasterMaterial);

    VkBufferMemoryBarrier postBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    postBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    postBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    postBarrier.buffer = m_objModel[upd.modelIndex].matColorBuffer.buffer;
    postBarrier.offset = offset;
    postBarrier.size = sizeof(RasterMaterial);

    postBarriers.push_back(postBarrier);
  }
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                       static_cast<uint32_t>(postBarriers.size()), postBarriers.data(), 0, nullptr);

  cmdGen.submitAndWait(cmdBuf);
}

void RasterGpuScene::createRayTracingResources()
{
  m_rtScene.build(m_objModel, m_instances);
}

void RasterGpuScene::destroyRayTracingResources()
{
  m_rtScene.destroy();
}

void RasterGpuScene::flushRayTracingUpdates()
{
  m_rtScene.flush(m_objModel, m_instances);
}

bool RasterGpuScene::hasRayTracingTlas() const
{
  return m_rtScene.hasTlas();
}

VkAccelerationStructureKHR RasterGpuScene::getRayTracingTlas() const
{
  return m_rtScene.getTlas();
}

std::optional<RasterTlasDescriptorInfo> RasterGpuScene::getRayTracingTlasDescriptorInfo() const
{
  return m_rtScene.getTlasDescriptorInfo();
}

void RasterGpuScene::createObjectDescriptionBuffer()
{
  nvvk::CommandPool cmdGen(m_device, m_graphicsQueueIndex);

  auto cmdBuf = cmdGen.createCommandBuffer();
  const std::vector<ObjDesc> dummyObjDesc(1);
  const std::vector<ObjDesc>& objDesc = m_objDesc.empty() ? dummyObjDesc : m_objDesc;
  m_alloc->destroy(m_bObjDesc);
  m_bObjDesc = m_alloc->createBuffer(cmdBuf, objDesc, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  cmdGen.submitAndWait(cmdBuf);
  m_alloc->finalizeAndReleaseStaging();
  m_debug->setObjectName(m_bObjDesc.buffer, "ObjDescs");
}

void RasterGpuScene::addLight(const Light& light)
{
  m_lights.push_back(light);
}

void RasterGpuScene::clearLights()
{
  m_lights.clear();
}

void RasterGpuScene::createLightBuffer()
{
  size_t maxLights = MAX_SCENE_LIGHTS;
  size_t bufferSize = sizeof(Light) * maxLights;

  m_alloc->destroy(m_bLights);
  m_bLights = m_alloc->createBuffer(bufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  m_debug->setObjectName(m_bLights.buffer, "Lights");
}

void RasterGpuScene::updateLightBuffer(const VkCommandBuffer& cmdBuf)
{
  const size_t lightCount = std::min<size_t>(m_lights.size(), MAX_SCENE_LIGHTS);
  if(lightCount > 0)
  {
    const VkDeviceSize lightBufferSize = sizeof(Light) * lightCount;

    VkBufferMemoryBarrier preBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    preBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    preBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    preBarrier.buffer = m_bLights.buffer;
    preBarrier.offset = 0;
    preBarrier.size = lightBufferSize;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr,
                         1, &preBarrier, 0, nullptr);

    vkCmdUpdateBuffer(cmdBuf, m_bLights.buffer, 0, lightBufferSize, m_lights.data());

    VkBufferMemoryBarrier postBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    postBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    postBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    postBarrier.buffer = m_bLights.buffer;
    postBarrier.offset = 0;
    postBarrier.size = lightBufferSize;
    vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                         1, &postBarrier, 0, nullptr);
  }
}

void RasterGpuScene::destroyTextures()
{
  if(m_alloc != nullptr)
  {
    for(auto& texture : m_textures)
    {
      m_alloc->destroy(texture);
    }
  }
  m_textures.clear();
}

void RasterGpuScene::destroyMeshBuffers()
{
  if(m_alloc != nullptr)
  {
    for(auto& model : m_objModel)
    {
      m_alloc->destroy(model.vertexBuffer);
      m_alloc->destroy(model.indexBuffer);
      m_alloc->destroy(model.matColorBuffer);
      m_alloc->destroy(model.matIndexBuffer);
    }
  }
  m_objModel.clear();
}
