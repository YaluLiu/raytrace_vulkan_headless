#include <sstream>

#define STB_IMAGE_IMPLEMENTATION
#include <array>
#include <cstddef>
#include <glm/glm.hpp>
#include <limits>
#include <type_traits>

#include "hello_vulkan.hpp"
#include "nvh/alignment.hpp"
#include "nvh/fileoperations.hpp"
#include "nvvk/buffers_vk.hpp"
#include "nvvk/commands_vk.hpp"
#include "nvvk/descriptorsets_vk.hpp"
#include "nvvk/images_vk.hpp"
#include "nvvk/pipeline_vk.hpp"
#include "nvvk/renderpasses_vk.hpp"
#include "stb_image.h"

extern std::vector<std::string> defaultSearchPaths;

static_assert(std::is_standard_layout_v<MaterialObj>);
static_assert(std::is_standard_layout_v<WaveFrontMaterial>);
static_assert(sizeof(MaterialObj) == sizeof(WaveFrontMaterial));
static_assert(sizeof(VertexObj) == sizeof(Vertex));
static_assert(offsetof(VertexObj, pos) == offsetof(Vertex, pos));
static_assert(offsetof(VertexObj, tangent) == offsetof(Vertex, tangent));
static_assert(offsetof(MaterialObj, baseColorFactor) == offsetof(WaveFrontMaterial, baseColorFactor));
static_assert(offsetof(MaterialObj, transmissionColorFactor) == offsetof(WaveFrontMaterial, transmissionColorFactor));
static_assert(offsetof(MaterialObj, roughnessFactor) == offsetof(WaveFrontMaterial, roughnessFactor));
static_assert(offsetof(MaterialObj, transmissionFactor) == offsetof(WaveFrontMaterial, transmissionFactor));
static_assert(offsetof(MaterialObj, subsurfaceFactor) == offsetof(WaveFrontMaterial, subsurfaceFactor));
static_assert(offsetof(MaterialObj, diffuseTextureId) == offsetof(WaveFrontMaterial, diffuseTextureId));
static_assert(offsetof(MaterialObj, normalTextureId) == offsetof(WaveFrontMaterial, normalTextureId));
static_assert(offsetof(MaterialObj, subsurfaceTextureId) == offsetof(WaveFrontMaterial, subsurfaceTextureId));

uint32_t HelloVulkan::addInstance(const glm::mat4& transform, uint32_t objIndex, int instanceId)
{
  ObjInstance instance;
  instance.transform = transform;
  instance.objIndex = objIndex;
  m_instances.push_back(instance);
  m_instanceIds.push_back(instanceId);
  resetFrameHistory();
  return static_cast<uint32_t>(m_instances.size() - 1);
}

void HelloVulkan::loadModel(ModelLoader& loader, glm::mat4 transform)
{
  for (auto& m : loader.m_materials)
  {
    m.ambient = glm::pow(m.ambient, glm::vec3(2.2f));
    m.diffuse = glm::pow(m.diffuse, glm::vec3(2.2f));
    m.specular = glm::pow(m.specular, glm::vec3(2.2f));
    m.emission = glm::pow(m.emission, glm::vec3(2.2f));
    m.baseColorFactor = glm::pow(m.baseColorFactor, glm::vec3(2.2f));
    m.emissionFactor = glm::pow(m.emissionFactor, glm::vec3(2.2f));
  }

  ObjModel model;
  model.nbIndices = static_cast<uint32_t>(loader.m_indices.size());
  model.nbVertices = static_cast<uint32_t>(loader.m_vertices.size());

  nvvk::CommandPool cmdBufGet(m_device, m_graphicsQueueIndex);
  VkCommandBuffer cmdBuf = cmdBufGet.createCommandBuffer();
  VkBufferUsageFlags flag = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  VkBufferUsageFlags rayTracingFlags =
      flag | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  model.vertexBuffer =
      m_alloc.createBuffer(cmdBuf, loader.m_vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | rayTracingFlags);
  model.indexBuffer =
      m_alloc.createBuffer(cmdBuf, loader.m_indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | rayTracingFlags);
  model.matColorBuffer = m_alloc.createBuffer(cmdBuf, loader.m_materials, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | flag);
  model.matIndexBuffer = m_alloc.createBuffer(cmdBuf, loader.m_matIndx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | flag);

  auto txtOffset = 0;
  createTextureImages(cmdBuf, loader.m_textures, loader.m_textureAssets);
  cmdBufGet.submitAndWait(cmdBuf);

  m_alloc.finalizeAndReleaseStaging();

  std::string objNb = std::to_string(m_objModel.size());
  m_debug.setObjectName(model.vertexBuffer.buffer, (std::string("vertex_" + objNb)));
  m_debug.setObjectName(model.indexBuffer.buffer, (std::string("index_" + objNb)));
  m_debug.setObjectName(model.matColorBuffer.buffer, (std::string("mat_" + objNb)));
  m_debug.setObjectName(model.matIndexBuffer.buffer, (std::string("matIdx_" + objNb)));

  addInstance(transform, static_cast<uint32_t>(m_objModel.size()), 0);

  ObjDesc desc;
  desc.txtOffset = txtOffset;
  desc.vertexAddress = nvvk::getBufferDeviceAddress(m_device, model.vertexBuffer.buffer);
  desc.indexAddress = nvvk::getBufferDeviceAddress(m_device, model.indexBuffer.buffer);
  desc.materialAddress = nvvk::getBufferDeviceAddress(m_device, model.matColorBuffer.buffer);
  desc.materialIndexAddress = nvvk::getBufferDeviceAddress(m_device, model.matIndexBuffer.buffer);

  m_objModel.emplace_back(model);
  m_objDesc.emplace_back(desc);
  m_Loader.emplace_back(loader);
  resetFrameHistory();
}

void HelloVulkan::createTextureImages(const VkCommandBuffer& cmdBuf, const std::vector<std::string>& textures,
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

    nvvk::Image image = m_alloc.createImage(cmdBuf, bufferSize, pixels, imageCreateInfo);
    nvvk::cmdGenerateMipmaps(cmdBuf, image.image, format, imgSize, imageCreateInfo.mipLevels);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, imageCreateInfo);
    nvvk::Texture texture = m_alloc.createTexture(image, ivInfo, samplerCreateInfo);

    m_textures.push_back(texture);
  };

  if (textures.empty() && textureAssets.empty() && m_textures.empty())
  {
    nvvk::Texture texture;

    std::array<uint8_t, 4> color{255u, 255u, 255u, 255u};
    VkDeviceSize bufferSize = sizeof(color);
    auto imgSize = VkExtent2D{1, 1};
    VkFormat format = formatForColorSpace(TextureColorSpace::SRGB);
    auto imageCreateInfo = nvvk::makeImage2DCreateInfo(imgSize, format);

    nvvk::Image image = m_alloc.createImage(cmdBuf, bufferSize, color.data(), imageCreateInfo);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, imageCreateInfo);
    texture = m_alloc.createTexture(image, ivInfo, samplerCreateInfo);

    nvvk::cmdBarrierImageLayout(cmdBuf, texture.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_textures.push_back(texture);
  }
  else
  {
    for (const auto& texture : textures)
    {
      std::stringstream o;
      int texWidth, texHeight, texChannels;
      o << "media/textures/" << texture;
      std::string txtFile = nvh::findFile(o.str(), defaultSearchPaths, true);

      stbi_uc* stbi_pixels = stbi_load(txtFile.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

      std::array<stbi_uc, 4> color{255u, 0u, 255u, 255u};

      stbi_uc* pixels = stbi_pixels;
      if (!stbi_pixels)
      {
        texWidth = texHeight = 1;
        texChannels = 4;
        pixels = reinterpret_cast<stbi_uc*>(color.data());
      }

      uploadTexture(pixels, texWidth, texHeight, TextureColorSpace::SRGB);

      stbi_image_free(stbi_pixels);
    }

    for (const TextureAsset& textureAsset : textureAssets)
    {
      int texWidth = 0;
      int texHeight = 0;
      int texChannels = 0;
      stbi_uc* stbi_pixels = nullptr;
      if (!textureAsset.encodedBytes.empty() &&
          textureAsset.encodedBytes.size() <= static_cast<size_t>(std::numeric_limits<int>::max()))
      {
        stbi_pixels =
            stbi_load_from_memory(textureAsset.encodedBytes.data(), static_cast<int>(textureAsset.encodedBytes.size()),
                                  &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
      }

      std::array<stbi_uc, 4> color{255u, 0u, 255u, 255u};

      stbi_uc* pixels = stbi_pixels;
      if (!stbi_pixels)
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
