#include <sstream>

#define STB_IMAGE_IMPLEMENTATION
#include <glm/glm.hpp>
#include "stb_image.h"

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

// opengl context with windows
#include "nvgl/contextwindow_gl.hpp"
void createOpenGLContext()
{
  glfwInit();
  // 设置 GLFW 窗口使用 OpenGL 4.5
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
  glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);  // 创建隐藏窗口
  // 创建 GLFW 窗口
  GLFWwindow* gl_window = glfwCreateWindow(1, 1, PROJECT_NAME, NULL, NULL);
  // 设置当前 OpenGL 上下文
  glfwMakeContextCurrent(gl_window);
  // 加载OpenGL函数
  load_GL(nvgl::ContextWindow::sysGetProcAddress);
}
//--------------------------------------------------------------------------------------------------
void HelloVulkan::setup(const VkInstance& instance, const VkDevice& device, const VkPhysicalDevice& physicalDevice, uint32_t queueFamily)
{
  // 调用基类的setup，初始化Vulkan低层对象
  AppOffline::setup(instance, device, physicalDevice, queueFamily);
  // 初始化资源分配器，用于设备上分配buffer/image等
  m_alloc.init(device, physicalDevice);
  // 初始化调试辅助功能（用于对象命名、调试标签等）
  m_debug.setup(m_device);
  // 查找适合的离屏深度格式
  m_offscreenDepthFormat = nvvk::findDepthFormat(physicalDevice);
#if ENABLE_GL_VK_CONVERSION
#if !ENABLE_HYDRA
  createOpenGLContext();
#endif
  m_allocGL.init(device, physicalDevice);
#endif
}

//--------------------------------------------------------------------------------------------------
// 每帧更新摄像机矩阵（view/proj/inverse等）到uniform buffer
// cmdBuf: 当前帧的命令缓冲
void HelloVulkan::updateUniformBuffer(const VkCommandBuffer& cmdBuf)
{
  // 计算当前窗口的宽高比
  const float    aspectRatio = m_size.width / static_cast<float>(m_size.height);
  GlobalUniforms hostUBO     = {};
  // 获取摄像机view矩阵
  const auto& view = CameraManip.getMatrix();
  // 构造右手坐标系的投影矩阵，并调整Y轴（Vulkan标准）
  glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(CameraManip.getFov()), aspectRatio, 0.1f, 1000.0f);
#if !ENABLE_HYDRA
  proj[1][1] *= -1;  // Vulkan坐标系Y反转
#endif

  // 填充UBO内容
  hostUBO.viewProj    = proj * view;
  hostUBO.viewInverse = glm::inverse(view);
  hostUBO.projInverse = glm::inverse(proj);

  // 设备端的UBO和所需的访问阶段
  VkBuffer deviceUBO      = m_bGlobals.buffer;
  auto     uboUsageStages = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR;

  // 屏障，保证写入前上帧不会读到
  VkBufferMemoryBarrier beforeBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  beforeBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  beforeBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  beforeBarrier.buffer        = deviceUBO;
  beforeBarrier.offset        = 0;
  beforeBarrier.size          = sizeof(hostUBO);
  vkCmdPipelineBarrier(cmdBuf, uboUsageStages, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0,
                       nullptr, 1, &beforeBarrier, 0, nullptr);

  // 用命令缓冲直接更新UBO数据（host -> device）
  vkCmdUpdateBuffer(cmdBuf, m_bGlobals.buffer, 0, sizeof(GlobalUniforms), &hostUBO);

  // 屏障，保证写入后可供shader读取
  VkBufferMemoryBarrier afterBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  afterBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  afterBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  afterBarrier.buffer        = deviceUBO;
  afterBarrier.offset        = 0;
  afterBarrier.size          = sizeof(hostUBO);
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT, uboUsageStages, VK_DEPENDENCY_DEVICE_GROUP_BIT, 0,
                       nullptr, 1, &afterBarrier, 0, nullptr);
}

//--------------------------------------------------------------------------------------------------
// 创建图形渲染用的描述符集布局
// 包括：全局UBO，物体描述buffer，所有纹理采样器
void HelloVulkan::createDescriptorSetLayout()
{
  auto nbTxt = static_cast<uint32_t>(m_textures.size());

  // 摄像机矩阵 UBO
  m_descSetLayoutBind.addBinding(SceneBindings::eGlobals, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_RAYGEN_BIT_KHR);
  // 物体描述 SSBO
  m_descSetLayoutBind.addBinding(SceneBindings::eObjDescs, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                 VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
  // 所有纹理采样器
  m_descSetLayoutBind.addBinding(SceneBindings::eTextures, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nbTxt,
                                 VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);

  // 创建layout和pool
  m_descSetLayout = m_descSetLayoutBind.createLayout(m_device);
  m_descPool      = m_descSetLayoutBind.createPool(m_device, 1);
  m_descSet       = nvvk::allocateDescriptorSet(m_device, m_descPool, m_descSetLayout);
}

//--------------------------------------------------------------------------------------------------
// 用实际的buffer/image填充描述符集
void HelloVulkan::updateDescriptorSet()
{
  std::vector<VkWriteDescriptorSet> writes;

  // 摄像机矩阵 UBO
  VkDescriptorBufferInfo dbiUnif{m_bGlobals.buffer, 0, VK_WHOLE_SIZE};
  writes.emplace_back(m_descSetLayoutBind.makeWrite(m_descSet, SceneBindings::eGlobals, &dbiUnif));

  // 物体描述 SSBO
  VkDescriptorBufferInfo dbiSceneDesc{m_bObjDesc.buffer, 0, VK_WHOLE_SIZE};
  writes.emplace_back(m_descSetLayoutBind.makeWrite(m_descSet, SceneBindings::eObjDescs, &dbiSceneDesc));

  // 纹理数组
  std::vector<VkDescriptorImageInfo> diit;
  for(auto& texture : m_textures)
  {
    diit.emplace_back(texture.descriptor);
  }
  writes.emplace_back(m_descSetLayoutBind.makeWriteArray(m_descSet, SceneBindings::eTextures, diit.data()));

  // 写入描述符集
  vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

// --------------------------------------------------------------------------------------------------
// 创建图形管线Layout及管线本体
void HelloVulkan::createGraphicsPipeline()
{
  // 配置push constant范围（支持vertex/fragment shader访问）
  VkPushConstantRange pushConstantRanges = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantRaster)};

  // 管线布局（包括描述符集和push constant）
  VkPipelineLayoutCreateInfo createInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  createInfo.setLayoutCount         = 1;
  createInfo.pSetLayouts            = &m_descSetLayout;
  createInfo.pushConstantRangeCount = 1;
  createInfo.pPushConstantRanges    = &pushConstantRanges;
  vkCreatePipelineLayout(m_device, &createInfo, nullptr, &m_pipelineLayout);

  // 构造图形管线
  std::vector<std::string>                paths = defaultSearchPaths;
  nvvk::GraphicsPipelineGeneratorCombined gpb(m_device, m_pipelineLayout, m_offscreenRenderPass);
  gpb.depthStencilState.depthTestEnable = true;
  // 加载SPIR-V shader
  gpb.addShader(nvh::loadFile("spv/vert_shader.vert.spv", true, paths, true), VK_SHADER_STAGE_VERTEX_BIT);
  gpb.addShader(nvh::loadFile("spv/frag_shader.frag.spv", true, paths, true), VK_SHADER_STAGE_FRAGMENT_BIT);
  // 顶点输入描述
  gpb.addBindingDescription({0, sizeof(VertexObj)});
  gpb.addAttributeDescriptions({
      {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(VertexObj, pos))},
      {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(VertexObj, nrm))},
      {2, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(VertexObj, color))},
      {3, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(VertexObj, texCoord))},
  });

  m_graphicsPipeline = gpb.createPipeline();
  m_debug.setObjectName(m_graphicsPipeline, "Graphics");
}

//--------------------------------------------------------------------------------------------------
// 加载OBJ模型，并构建顶点、索引、材质等buffer
// filename: obj文件路径
// transform: 模型实例变换矩阵
void HelloVulkan::loadModel(ModelLoader& loader, glm::mat4 transform)
{
  // 将材质颜色从SRGB空间转换到线性空间
  for(auto& m : loader.m_materials)
  {
    m.ambient  = glm::pow(m.ambient, glm::vec3(2.2f));
    m.diffuse  = glm::pow(m.diffuse, glm::vec3(2.2f));
    m.specular = glm::pow(m.specular, glm::vec3(2.2f));
  }

  ObjModel model;
  model.nbIndices  = static_cast<uint32_t>(loader.m_indices.size());
  model.nbVertices = static_cast<uint32_t>(loader.m_vertices.size());

  // 在设备上创建并上传顶点、索引、材质等buffer
  nvvk::CommandPool  cmdBufGet(m_device, m_graphicsQueueIndex);
  VkCommandBuffer    cmdBuf          = cmdBufGet.createCommandBuffer();
  VkBufferUsageFlags flag            = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  VkBufferUsageFlags rayTracingFlags =  // 用于光追加速结构构建
      flag | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  model.vertexBuffer = m_alloc.createBuffer(cmdBuf, loader.m_vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | rayTracingFlags);
  model.indexBuffer = m_alloc.createBuffer(cmdBuf, loader.m_indices, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | rayTracingFlags);
  model.matColorBuffer = m_alloc.createBuffer(cmdBuf, loader.m_materials, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | flag);
  model.matIndexBuffer = m_alloc.createBuffer(cmdBuf, loader.m_matIndx, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | flag);

  // 纹理贴图（若有），并记录偏移
  // 不记录偏移，全部mesh复用第一个mesh的贴图，作为全局贴图
  // todo: add global textures
  auto txtOffset = static_cast<uint32_t>(m_textures.size());
  createTextureImages(cmdBuf, loader.m_textures);
  cmdBufGet.submitAndWait(cmdBuf);

  // 分配mesh和texture完毕
  m_alloc.finalizeAndReleaseStaging();

  std::string objNb = std::to_string(m_objModel.size());
  m_debug.setObjectName(model.vertexBuffer.buffer, (std::string("vertex_" + objNb)));
  m_debug.setObjectName(model.indexBuffer.buffer, (std::string("index_" + objNb)));
  m_debug.setObjectName(model.matColorBuffer.buffer, (std::string("mat_" + objNb)));
  m_debug.setObjectName(model.matIndexBuffer.buffer, (std::string("matIdx_" + objNb)));

  // 生成实例信息
  ObjInstance instance;
  instance.transform = transform;
  instance.objIndex  = static_cast<uint32_t>(m_objModel.size());
  m_instances.push_back(instance);

  // 构造设备可访问的物体描述
  ObjDesc desc;
  desc.txtOffset            = txtOffset;
  desc.vertexAddress        = nvvk::getBufferDeviceAddress(m_device, model.vertexBuffer.buffer);
  desc.indexAddress         = nvvk::getBufferDeviceAddress(m_device, model.indexBuffer.buffer);
  desc.materialAddress      = nvvk::getBufferDeviceAddress(m_device, model.matColorBuffer.buffer);
  desc.materialIndexAddress = nvvk::getBufferDeviceAddress(m_device, model.matIndexBuffer.buffer);

  // 存储模型与描述
  m_objModel.emplace_back(model);
  m_objDesc.emplace_back(desc);
  m_Loader.emplace_back(loader);
}

//--------------------------------------------------------------------------------------------------
// 创建uniform buffer（摄像机矩阵等），显存可见
void HelloVulkan::createUniformBuffer()
{
  m_bGlobals = m_alloc.createBuffer(sizeof(GlobalUniforms), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
  m_debug.setObjectName(m_bGlobals.buffer, "Globals");
}

//--------------------------------------------------------------------------------------------------
// 创建物体描述buffer（描述场景中各物体、变换、纹理偏移等）
// 用于shader侧访问
void HelloVulkan::createObjDescriptionBuffer()
{
  nvvk::CommandPool cmdGen(m_device, m_graphicsQueueIndex);

  auto cmdBuf = cmdGen.createCommandBuffer();
  m_bObjDesc  = m_alloc.createBuffer(cmdBuf, m_objDesc, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
  cmdGen.submitAndWait(cmdBuf);
  m_alloc.finalizeAndReleaseStaging();
  m_debug.setObjectName(m_bObjDesc.buffer, "ObjDescs");
}

//--------------------------------------------------------------------------------------------------
// 创建所有纹理贴图和采样器，并上传到GPU
// cmdBuf: 用于资源上传的命令缓冲
// textures: 纹理文件名数组
void HelloVulkan::createTextureImages(const VkCommandBuffer& cmdBuf, const std::vector<std::string>& textures)
{
  VkSamplerCreateInfo samplerCreateInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
  samplerCreateInfo.minFilter  = VK_FILTER_LINEAR;
  samplerCreateInfo.magFilter  = VK_FILTER_LINEAR;
  samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  samplerCreateInfo.maxLod     = FLT_MAX;

  VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;

  // 若没有任何纹理，为了兼容pipeline，创建一个dummy白色纹理
  if(textures.empty() && m_textures.empty())
  {
    nvvk::Texture texture;

    std::array<uint8_t, 4> color{255u, 255u, 255u, 255u};
    VkDeviceSize           bufferSize      = sizeof(color);
    auto                   imgSize         = VkExtent2D{1, 1};
    auto                   imageCreateInfo = nvvk::makeImage2DCreateInfo(imgSize, format);

    nvvk::Image           image  = m_alloc.createImage(cmdBuf, bufferSize, color.data(), imageCreateInfo);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, imageCreateInfo);
    texture                      = m_alloc.createTexture(image, ivInfo, samplerCreateInfo);

    nvvk::cmdBarrierImageLayout(cmdBuf, texture.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    m_textures.push_back(texture);
  }
  else
  {
    // 批量加载所有图片
    for(const auto& texture : textures)
    {
      std::stringstream o;
      int               texWidth, texHeight, texChannels;
      o << "media/textures/" << texture;
      std::string txtFile = nvh::findFile(o.str(), defaultSearchPaths, true);

      stbi_uc* stbi_pixels = stbi_load(txtFile.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

      std::array<stbi_uc, 4> color{255u, 0u, 255u, 255u};

      stbi_uc* pixels = stbi_pixels;
      // 兜底：加载失败时用紫色
      if(!stbi_pixels)
      {
        texWidth = texHeight = 1;
        texChannels          = 4;
        pixels               = reinterpret_cast<stbi_uc*>(color.data());
      }

      VkDeviceSize bufferSize      = static_cast<uint64_t>(texWidth) * texHeight * sizeof(uint8_t) * 4;
      auto         imgSize         = VkExtent2D{(uint32_t)texWidth, (uint32_t)texHeight};
      auto         imageCreateInfo = nvvk::makeImage2DCreateInfo(imgSize, format, VK_IMAGE_USAGE_SAMPLED_BIT, true);

      {
        nvvk::Image image = m_alloc.createImage(cmdBuf, bufferSize, pixels, imageCreateInfo);
        nvvk::cmdGenerateMipmaps(cmdBuf, image.image, format, imgSize, imageCreateInfo.mipLevels);
        VkImageViewCreateInfo ivInfo  = nvvk::makeImageViewCreateInfo(image.image, imageCreateInfo);
        nvvk::Texture         texture = m_alloc.createTexture(image, ivInfo, samplerCreateInfo);

        m_textures.push_back(texture);
      }

      stbi_image_free(stbi_pixels);
    }
  }
}

//--------------------------------------------------------------------------------------------------
// 释放销毁所有分配的资源，包括管线、buffer、图片、加速结构等
// headless:检查资源有效性，仅销毁已创建的资源。
void HelloVulkan::destroyResources()
{
  // 图形管线相关
  vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
  vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
  vkDestroyDescriptorPool(m_device, m_descPool, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_descSetLayout, nullptr);

  m_alloc.destroy(m_bGlobals);
  m_alloc.destroy(m_bObjDesc);

  for(auto& m : m_objModel)
  {
    m_alloc.destroy(m.vertexBuffer);
    m_alloc.destroy(m.indexBuffer);
    m_alloc.destroy(m.matColorBuffer);
    m_alloc.destroy(m.matIndexBuffer);
  }

  for(auto& t : m_textures)
  {
    m_alloc.destroy(t);
  }

#if ENABLE_GL_VK_CONVERSION
  m_rtOutputGL.destroy(m_allocGL);
  m_rtObjectIdGL.destroy(m_allocGL);
  vkDestroySemaphore(m_device, m_semaphores.vkComplete, nullptr);
  vkDestroySemaphore(m_device, m_semaphores.vkReady, nullptr);
#else
  //#Post处理相关
  m_allocGL.destroy(m_offscreenColor);
  m_allocGL.destroy(m_offscreenDepth);
#endif
  m_allocGL.deinit();

  vkDestroyRenderPass(m_device, m_offscreenRenderPass, nullptr);
  vkDestroyFramebuffer(m_device, m_offscreenFramebuffer, nullptr);

  // #VKRay 光线追踪相关
  m_rtBuilder.destroy();
  m_sbtWrapper.destroy();
  vkDestroyPipeline(m_device, m_rtPipeline, nullptr);
  vkDestroyPipelineLayout(m_device, m_rtPipelineLayout, nullptr);
  vkDestroyDescriptorPool(m_device, m_rtDescPool, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_rtDescSetLayout, nullptr);

  // #VK_compute 计算着色器相关
  vkDestroyPipeline(m_device, m_compPipeline, nullptr);
  vkDestroyPipelineLayout(m_device, m_compPipelineLayout, nullptr);
  vkDestroyDescriptorPool(m_device, m_compDescPool, nullptr);
  vkDestroyDescriptorSetLayout(m_device, m_compDescSetLayout, nullptr);

  m_alloc.deinit();
}

//--------------------------------------------------------------------------------------------------
// 窗口大小变化时的回调：重建离屏渲染、后处理和光追相关的描述符集
// w, h: 新窗口宽高（未使用）
//headless:模式下一般不会有窗口 resize
void HelloVulkan::onResize(int w, int h)
{
  if(w == (int)m_size.width && h == (int)m_size.height)
    return;
  m_size.width  = w;
  m_size.height = h;
  // 重建离屏渲染（包括color/depth framebuffer、renderpass等）
  createOffscreenRender();
  // 更新后处理描述符集（采样新的offscreen image）
  // updatePostDescriptorSet();
  // 更新光线追踪输出描述符集（采样新的offscreen image）
  updateRtDescriptorSet();
}

//--------------------------------------------------------------------------------------------------
// 创建离屏渲染所需的framebuffer、renderpass和相关image资源
// 包括color和depth两张image，适配当前窗口大小
void HelloVulkan::createOffscreenRender()
{
  // 释放旧的离屏color和depth资源

  createOutputImage();
  createObjectIdImage();
  // 创建depth image和image view
  m_alloc.destroy(m_offscreenDepth);
  auto depthCreateInfo = nvvk::makeImage2DCreateInfo(m_size, m_offscreenDepthFormat, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
  {
    nvvk::Image image = m_alloc.createImage(depthCreateInfo);

    VkImageViewCreateInfo depthStencilView{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    depthStencilView.viewType         = VK_IMAGE_VIEW_TYPE_2D;
    depthStencilView.format           = m_offscreenDepthFormat;
    depthStencilView.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};
    depthStencilView.image            = image.image;

    m_offscreenDepth = m_alloc.createTexture(image, depthStencilView);
  }


  // 设置color和depth image的初始布局
  {
    nvvk::CommandPool genCmdBuf(m_device, m_graphicsQueueIndex);
    auto              cmdBuf = genCmdBuf.createCommandBuffer();
    nvvk::cmdBarrierImageLayout(cmdBuf, m_offscreenColor.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    nvvk::cmdBarrierImageLayout(cmdBuf, m_offscreenDepth.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
    nvvk::cmdBarrierImageLayout(cmdBuf, m_offscreenObjectId.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                                VK_IMAGE_ASPECT_COLOR_BIT);

    genCmdBuf.submitAndWait(cmdBuf);
  }

  // 创建离屏renderpass（只创建一次，除非首次调用）
  if(!m_offscreenRenderPass)
  {
    m_offscreenRenderPass = nvvk::createRenderPass(m_device, {m_offscreenColorFormat}, m_offscreenDepthFormat, 1, true,
                                                   true, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL);
  }

  // 创建离屏framebuffer，绑定color/depth两个attachment
  std::vector<VkImageView> attachments = {m_offscreenColor.descriptor.imageView, m_offscreenDepth.descriptor.imageView};

  vkDestroyFramebuffer(m_device, m_offscreenFramebuffer, nullptr);
  VkFramebufferCreateInfo info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
  info.renderPass      = m_offscreenRenderPass;
  info.attachmentCount = 2;
  info.pAttachments    = attachments.data();
  info.width           = m_size.width;
  info.height          = m_size.height;
  info.layers          = 1;
  vkCreateFramebuffer(m_device, &info, nullptr, &m_offscreenFramebuffer);
}

//--------------------------------------------------------------------------------------------------
// 初始化Vulkan光线追踪相关功能
// - 查询物理设备支持的光追属性
// - 配置BLAS/TLAS构建器和SBT封装器
void HelloVulkan::initRayTracing()
{
  // 查询物理设备光线追踪属性（如最大SBT大小、最大递归深度等）
  VkPhysicalDeviceProperties2 prop2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
  prop2.pNext = &m_rtProperties;
  vkGetPhysicalDeviceProperties2(m_physicalDevice, &prop2);

  // 初始化BLAS/TLAS构建器
  m_rtBuilder.setup(m_device, &m_alloc, m_graphicsQueueIndex);
  // 初始化SBT封装器
  m_sbtWrapper.setup(m_device, m_graphicsQueueIndex, &m_alloc, m_rtProperties);
}

//--------------------------------------------------------------------------------------------------
// 将一个OBJ模型转为Vulkan光追BLAS所需的Geometry结构
// 返回：nvvk::RaytracingBuilderKHR::BlasInput
auto HelloVulkan::objectToVkGeometryKHR(const ObjModel& model)
{
  // 获取顶点和索引buffer的设备地址
  VkDeviceAddress vertexAddress = nvvk::getBufferDeviceAddress(m_device, model.vertexBuffer.buffer);
  VkDeviceAddress indexAddress  = nvvk::getBufferDeviceAddress(m_device, model.indexBuffer.buffer);

  uint32_t maxPrimitiveCount = model.nbIndices / 3;

  // 设置三角形数据（顶点格式、地址、步长、索引类型等）
  VkAccelerationStructureGeometryTrianglesDataKHR triangles{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
  triangles.vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT;  // 顶点为vec3
  triangles.vertexData.deviceAddress = vertexAddress;
  triangles.vertexStride             = sizeof(VertexObj);
  triangles.indexType                = VK_INDEX_TYPE_UINT32;
  triangles.indexData.deviceAddress  = indexAddress;
  triangles.maxVertex                = model.nbVertices - 1;

  // 设置几何体结构体，描述为三角形且为opaque类型
  VkAccelerationStructureGeometryKHR asGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
  asGeom.geometryType       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  asGeom.flags              = VK_GEOMETRY_OPAQUE_BIT_KHR;
  asGeom.geometry.triangles = triangles;

  // BLAS构建范围
  VkAccelerationStructureBuildRangeInfoKHR offset;
  offset.firstVertex     = 0;
  offset.primitiveCount  = maxPrimitiveCount;
  offset.primitiveOffset = 0;
  offset.transformOffset = 0;

  nvvk::RaytracingBuilderKHR::BlasInput input;
  input.asGeometry.emplace_back(asGeom);
  input.asBuildOffsetInfo.emplace_back(offset);

  return input;
}

//--------------------------------------------------------------------------------------------------
// 创建所有物体的BLAS（底层加速结构）
// - 每个ObjModel创建一个BLAS
void HelloVulkan::createBottomLevelAS()
{
  // 预分配空间
  m_blas.reserve(m_objModel.size());
  // 遍历每个模型，生成BLAS输入
  for(const auto& obj : m_objModel)
  {
    auto blas = objectToVkGeometryKHR(obj);
    m_blas.push_back(blas);
  }
  // 构建所有BLAS，允许更新和快速构建
  m_rtBuilder.buildBlas(m_blas, VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR
                                    | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR);
}

//--------------------------------------------------------------------------------------------------
// 创建TLAS（顶层加速结构），将所有实例信息传入
// - 每个ObjInstance对应一个TLAS实例
void HelloVulkan::createTopLevelAS()
{
  for(const HelloVulkan::ObjInstance& inst : m_instances)
  {
    VkAccelerationStructureInstanceKHR rayInst{};
    rayInst.transform                              = nvvk::toTransformMatrixKHR(inst.transform);  // 实例变换矩阵
    rayInst.instanceCustomIndex                    = inst.objIndex;  // 自定义索引用于shader区分
    rayInst.accelerationStructureReference         = m_rtBuilder.getBlasDeviceAddress(inst.objIndex);  // BLAS地址
    rayInst.flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    rayInst.mask                                   = 0xFF;  // 所有射线都能命中
    rayInst.instanceShaderBindingTableRecordOffset = 0;     // 所有实例共用同一hitgroup
    m_tlas.emplace_back(rayInst);
  }

  // 设置TLAS构建标志（优先快速追踪，允许更新）
  m_rtFlags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
  m_rtBuilder.buildTlas(m_tlas, m_rtFlags);
}

//--------------------------------------------------------------------------------------------------
// 创建专用于光追的描述符集（TLAS和输出图像）
// - 包括TLAS句柄和output storage image
void HelloVulkan::createRtDescriptorSet()
{
  // 添加TLAS绑定（光追着色器可见）
  m_rtDescSetLayoutBind.addBinding(RtxBindings::eTlas, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
                                   VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR);
  // 添加输出图像绑定（只在raygen可见）
  m_rtDescSetLayoutBind.addBinding(RtxBindings::eOutImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR);
  // NEW: objectId image
  m_rtDescSetLayoutBind.addBinding(RtxBindings::eObjIdImage, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1, VK_SHADER_STAGE_RAYGEN_BIT_KHR);

  // 创建描述符池和布局
  m_rtDescPool      = m_rtDescSetLayoutBind.createPool(m_device);
  m_rtDescSetLayout = m_rtDescSetLayoutBind.createLayout(m_device);

  // 分配描述符集
  VkDescriptorSetAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
  allocateInfo.descriptorPool     = m_rtDescPool;
  allocateInfo.descriptorSetCount = 1;
  allocateInfo.pSetLayouts        = &m_rtDescSetLayout;
  vkAllocateDescriptorSets(m_device, &allocateInfo, &m_rtDescSet);

  // 填充TLAS和输出图像信息
  VkAccelerationStructureKHR tlas = m_rtBuilder.getAccelerationStructure();
  VkWriteDescriptorSetAccelerationStructureKHR descASInfo{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR};
  descASInfo.accelerationStructureCount = 1;
  descASInfo.pAccelerationStructures    = &tlas;
  VkDescriptorImageInfo imageInfo{{}, m_offscreenColor.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorImageInfo idInfo{{}, m_offscreenObjectId.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};

  std::vector<VkWriteDescriptorSet> writes;
  writes.emplace_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eTlas, &descASInfo));
  writes.emplace_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eOutImage, &imageInfo));
  writes.emplace_back(m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eObjIdImage, &idInfo));
  vkUpdateDescriptorSets(m_device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
}

//--------------------------------------------------------------------------------------------------
// 更新光线追踪描述符集中的输出图像（离屏渲染结果）
// - 当窗口resize或offscreen image重建后必须调用
void HelloVulkan::updateRtDescriptorSet()
{
  VkDescriptorImageInfo colorInfo{{}, m_offscreenColor.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};
  VkDescriptorImageInfo idInfo{{}, m_offscreenObjectId.descriptor.imageView, VK_IMAGE_LAYOUT_GENERAL};

  VkWriteDescriptorSet wColor = m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eOutImage, &colorInfo);
  VkWriteDescriptorSet wId    = m_rtDescSetLayoutBind.makeWrite(m_rtDescSet, RtxBindings::eObjIdImage, &idInfo);
  VkWriteDescriptorSet ws[2]  = {wColor, wId};
  vkUpdateDescriptorSets(m_device, 2, ws, 0, nullptr);
}

//--------------------------------------------------------------------------------------------------
// 创建光线追踪管线（Ray Tracing Pipeline）
void HelloVulkan::createRtPipeline()
{
  // 枚举每种shader在组中的索引
  enum StageIndices
  {
    eRaygen,
    eMiss,
    eMiss2,
    eClosestHit,
    eShaderGroupCount
  };

  // 1. 加载shader模块并创建VkPipelineShaderStageCreateInfo数组
  std::array<VkPipelineShaderStageCreateInfo, eShaderGroupCount> stages{};
  VkPipelineShaderStageCreateInfo stage{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};
  stage.pName = "main";  // 所有shader入口均为main

  // Raygen shader（主射线生成着色器）
  stage.module = nvvk::createShaderModule(m_device, nvh::loadFile("spv/raytrace.rgen.spv", true, defaultSearchPaths, true));
  stage.stage     = VK_SHADER_STAGE_RAYGEN_BIT_KHR;
  stages[eRaygen] = stage;

  // Miss shader（主射线未命中时调用）
  stage.module = nvvk::createShaderModule(m_device, nvh::loadFile("spv/raytrace.rmiss.spv", true, defaultSearchPaths, true));
  stage.stage   = VK_SHADER_STAGE_MISS_BIT_KHR;
  stages[eMiss] = stage;

  // Shadow Miss shader（二次/阴影射线未命中时调用，判断是否被遮挡）
  stage.module =
      nvvk::createShaderModule(m_device, nvh::loadFile("spv/raytraceShadow.rmiss.spv", true, defaultSearchPaths, true));
  stage.stage    = VK_SHADER_STAGE_MISS_BIT_KHR;
  stages[eMiss2] = stage;

  // Closest Hit shader（主射线命中三角面时调用）
  stage.module = nvvk::createShaderModule(m_device, nvh::loadFile("spv/raytrace.rchit.spv", true, defaultSearchPaths, true));
  stage.stage         = VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
  stages[eClosestHit] = stage;

  // 2. 配置Shader Group
  VkRayTracingShaderGroupCreateInfoKHR group{VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR};
  group.anyHitShader       = VK_SHADER_UNUSED_KHR;
  group.closestHitShader   = VK_SHADER_UNUSED_KHR;
  group.generalShader      = VK_SHADER_UNUSED_KHR;
  group.intersectionShader = VK_SHADER_UNUSED_KHR;

  // Raygen group
  group.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  group.generalShader = eRaygen;
  m_rtShaderGroups.push_back(group);

  // Miss group
  group.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  group.generalShader = eMiss;
  m_rtShaderGroups.push_back(group);

  // Shadow Miss group
  group.type          = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR;
  group.generalShader = eMiss2;
  m_rtShaderGroups.push_back(group);

  // Closest Hit group
  group.type             = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR;
  group.generalShader    = VK_SHADER_UNUSED_KHR;
  group.closestHitShader = eClosestHit;
  m_rtShaderGroups.push_back(group);

  // 3. 配置Push Constant，用于传递光线追踪参数（如光源、清屏色等）
  VkPushConstantRange pushConstant{VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
                                   0, sizeof(PushConstantRay)};

  // 4. 创建Pipeline Layout（包含光追和通用描述符集以及push constant）
  VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
  pipelineLayoutCreateInfo.pushConstantRangeCount = 1;
  pipelineLayoutCreateInfo.pPushConstantRanges    = &pushConstant;

  // 两个描述符集：一个专供光追（TLAS、存储图像），一个通用（UBO、纹理等）
  std::vector<VkDescriptorSetLayout> rtDescSetLayouts = {m_rtDescSetLayout, m_descSetLayout};
  pipelineLayoutCreateInfo.setLayoutCount             = static_cast<uint32_t>(rtDescSetLayouts.size());
  pipelineLayoutCreateInfo.pSetLayouts                = rtDescSetLayouts.data();

  vkCreatePipelineLayout(m_device, &pipelineLayoutCreateInfo, nullptr, &m_rtPipelineLayout);

  // 5. 创建Ray Tracing Pipeline对象
  VkRayTracingPipelineCreateInfoKHR rayPipelineInfo{VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR};
  rayPipelineInfo.stageCount = static_cast<uint32_t>(stages.size());  // 所有着色器阶段
  rayPipelineInfo.pStages    = stages.data();
  rayPipelineInfo.groupCount = static_cast<uint32_t>(m_rtShaderGroups.size());  // 所有shader group
  rayPipelineInfo.pGroups    = m_rtShaderGroups.data();

  // 设置递归深度（如主射线+阴影射线=2即可，太大影响性能）
  rayPipelineInfo.maxPipelineRayRecursionDepth = 2;
  rayPipelineInfo.layout                       = m_rtPipelineLayout;

  vkCreateRayTracingPipelinesKHR(m_device, {}, {}, 1, &rayPipelineInfo, nullptr, &m_rtPipeline);

  // 6. 创建SBT（Shader Binding Table），用于vkCmdTraceRays调度shader
  m_sbtWrapper.create(m_rtPipeline, rayPipelineInfo);

  // 清理shader模块资源
  for(auto& s : stages)
    vkDestroyShaderModule(m_device, s.module, nullptr);
}

//--------------------------------------------------------------------------------------------------
// 执行光线追踪渲染主流程（生成光追图像到offscreen image）
void HelloVulkan::raytrace(const VkCommandBuffer& cmdBuf, const glm::vec4& clearColor)
{
  m_debug.beginLabel(cmdBuf, "Ray trace");
  // 1. 初始化push constant内容
  m_pcRay.clearColor     = clearColor;
  m_pcRay.lightPosition  = m_pcRaster.lightPosition;
  m_pcRay.lightIntensity = m_pcRaster.lightIntensity;
  m_pcRay.lightType      = m_pcRaster.lightType;

  // 2. 绑定管线与描述符集
  std::vector<VkDescriptorSet> descSets{m_rtDescSet, m_descSet};
  vkCmdBindPipeline(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rtPipeline);
  // 绑定两个描述符集：光追和场景通用
  vkCmdBindDescriptorSets(cmdBuf, VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR, m_rtPipelineLayout, 0,
                          (uint32_t)descSets.size(), descSets.data(), 0, nullptr);
  // 绑定push constant数据
  vkCmdPushConstants(cmdBuf, m_rtPipelineLayout,
                     VK_SHADER_STAGE_RAYGEN_BIT_KHR | VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR | VK_SHADER_STAGE_MISS_BIT_KHR,
                     0, sizeof(PushConstantRay), &m_pcRay);

  // 3. 获取SBT各区域信息
  auto& regions = m_sbtWrapper.getRegions();
  // 4. 发射光线（每像素一条主射线，width*height次）
  vkCmdTraceRaysKHR(cmdBuf, &regions[0], &regions[1], &regions[2], &regions[3], m_size.width, m_size.height, 1);

  m_debug.endLabel(cmdBuf);
}

//--------------------------------------------------------------------------------------------------
// update blas & tlas
//--------------------------------------------------------------------------------------------------
void HelloVulkan::updateTlas(uint32_t mesh_Id, glm::mat4 transform, bool visible)
{
  VkAccelerationStructureInstanceKHR& tinst = m_tlas[mesh_Id];
  tinst.mask                                = visible ? 0xFF : 0x00;
  tinst.transform                           = nvvk::toTransformMatrixKHR(transform);
}

void HelloVulkan::updateTlasEnd()
{
  // Updating the top level acceleration structure
  m_rtBuilder.buildTlas(m_tlas, m_rtFlags, true);
}

// 动画处理球体对象的顶点，在 C++ 端进行缩放
void HelloVulkan::updateBlas(uint32_t mesh_Id)
{
  //更新了loader的对应顶点数据
  std::vector<VertexObj>& now_vertices = m_Loader[mesh_Id].m_vertices;
  ObjModel&               model        = m_objModel[mesh_Id];

  // 创建命令池和命令缓冲区
  nvvk::CommandPool genCmdBuf(m_device, m_graphicsQueueIndex);
  VkCommandBuffer   cmdBuf = genCmdBuf.createCommandBuffer();

  // 更新模型的顶点数量
  model.nbVertices = static_cast<uint32_t>(now_vertices.size());


  // 定义缓冲区使用标志
  VkBufferUsageFlags flag = VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
  VkBufferUsageFlags rayTracingFlags =
      flag | VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;

  // 销毁旧的顶点缓冲区，防止内存泄漏
  m_alloc.destroy(model.vertexBuffer);

  // 创建新的顶点缓冲区并上传修改后的顶点数据
  model.vertexBuffer = m_alloc.createBuffer(cmdBuf, now_vertices, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | rayTracingFlags);

  // 提交命令缓冲区并等待执行完成
  genCmdBuf.submitAndWait(cmdBuf);

  // 更新底层加速结构（BLAS），使用新的顶点缓冲区
  m_rtBuilder.updateBlas(mesh_Id, m_blas[mesh_Id],
                         VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR);
}

//--------------------------------------------------------------------------------------------------
// update material
//--------------------------------------------------------------------------------------------------
// 在渲染循环中修改材质
void HelloVulkan::updateMaterialAtRuntime(int modelIndex, int materialIndex, const MaterialObj& newMaterial)
{
  nvvk::CommandPool cmdGen(m_device, m_graphicsQueueIndex);
  VkCommandBuffer   cmdBuf = cmdGen.createCommandBuffer();

  // 计算偏移量
  VkDeviceSize offset = materialIndex * sizeof(MaterialObj);

  // 确保缓冲区可见性
  VkBufferMemoryBarrier barrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
  barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.buffer        = m_objModel[modelIndex].matColorBuffer.buffer;
  barrier.offset        = offset;
  barrier.size          = sizeof(MaterialObj);

  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 1, &barrier, 0, nullptr);

  // 更新材质数据
  vkCmdUpdateBuffer(cmdBuf, m_objModel[modelIndex].matColorBuffer.buffer, offset, sizeof(MaterialObj), &newMaterial);

  // 确保更新后的数据对着色器可见
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0,
                       nullptr, 1, &barrier, 0, nullptr);

  cmdGen.submitAndWait(cmdBuf);
}

void HelloVulkan::updateMaterialsAtRuntime(const std::vector<MaterialUpdate>& updates)
{
  // 用一个命令池/命令缓冲，减少同步消耗
  nvvk::CommandPool cmdGen(m_device, m_graphicsQueueIndex);
  VkCommandBuffer   cmdBuf = cmdGen.createCommandBuffer();

  std::vector<VkBufferMemoryBarrier> preBarriers;
  std::vector<VkBufferMemoryBarrier> postBarriers;

  // 1. 先加所有Barrier，保证并行性
  for(const auto& upd : updates)
  {
    VkDeviceSize offset = upd.materialIndex * sizeof(MaterialObj);

    VkBufferMemoryBarrier preBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    preBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    preBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    preBarrier.buffer        = m_objModel[upd.modelIndex].matColorBuffer.buffer;
    preBarrier.offset        = offset;
    preBarrier.size          = sizeof(MaterialObj);

    preBarriers.push_back(preBarrier);
  }
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, static_cast<uint32_t>(preBarriers.size()),
                       preBarriers.data(), 0, nullptr);

  // 2. 更新所有材质
  for(const auto& upd : updates)
  {
    VkDeviceSize offset = upd.materialIndex * sizeof(MaterialObj);
    vkCmdUpdateBuffer(cmdBuf, m_objModel[upd.modelIndex].matColorBuffer.buffer, offset, sizeof(MaterialObj), &upd.newMaterial);
  }

  // 3. 再加所有postBarrier，保证对shader可见
  for(const auto& upd : updates)
  {
    VkDeviceSize offset = upd.materialIndex * sizeof(MaterialObj);

    VkBufferMemoryBarrier postBarrier{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};
    postBarrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    postBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    postBarrier.buffer        = m_objModel[upd.modelIndex].matColorBuffer.buffer;
    postBarrier.offset        = offset;
    postBarrier.size          = sizeof(MaterialObj);

    postBarriers.push_back(postBarrier);
  }
  vkCmdPipelineBarrier(cmdBuf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR, 0, 0,
                       nullptr, static_cast<uint32_t>(postBarriers.size()), postBarriers.data(), 0, nullptr);

  // 4. 提交命令
  cmdGen.submitAndWait(cmdBuf);
}

void HelloVulkan::createOutputImage()
{
#if ENABLE_GL_VK_CONVERSION
  m_rtOutputGL.destroy(m_allocGL);
#else
  m_allocGL.destroy(m_offscreenColor);
#endif

  {
    auto CreateInfo = nvvk::makeImage2DCreateInfo(m_size, m_offscreenColorFormat,
                                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                                      | VK_IMAGE_USAGE_STORAGE_BIT);

    nvvk::Image           image  = m_allocGL.createImage(CreateInfo);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, CreateInfo);
    VkSamplerCreateInfo   sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    m_offscreenColor                        = m_allocGL.createTexture(image, ivInfo, sampler);
    m_offscreenColor.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  }

#if ENABLE_GL_VK_CONVERSION
  m_rtOutputGL.imgSize = m_size;
  m_rtOutputGL.texVk   = m_offscreenColor;
  createTextureGL(m_rtOutputGL, GL_RGBA32F, GL_LINEAR, GL_LINEAR, GL_CLAMP_TO_EDGE, m_allocGL);
#endif
}

void HelloVulkan::createObjectIdImage()
{
#if ENABLE_GL_VK_CONVERSION
  m_rtObjectIdGL.destroy(m_allocGL);
#else
  m_allocGL.destroy(m_offscreenObjectId);
#endif
  {
    auto CreateInfo = nvvk::makeImage2DCreateInfo(m_size, m_offscreenObjectIdFormat,
                                                  VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT
                                                      | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                                      | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

    nvvk::Image           image  = m_allocGL.createImage(CreateInfo);
    VkImageViewCreateInfo ivInfo = nvvk::makeImageViewCreateInfo(image.image, CreateInfo);
    VkSamplerCreateInfo   sampler{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    m_offscreenObjectId                        = m_allocGL.createTexture(image, ivInfo, sampler);
    m_offscreenObjectId.descriptor.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
  }
#if ENABLE_GL_VK_CONVERSION
  m_rtObjectIdGL.imgSize   = m_size;
  m_rtObjectIdGL.texVk     = m_offscreenObjectId;
  m_rtObjectIdGL.mipLevels = 1;
  createTextureGL(m_rtObjectIdGL, GL_R32I, GL_NEAREST, GL_NEAREST, GL_CLAMP_TO_EDGE, m_allocGL);
#endif
}

void HelloVulkan::submitFrame()
{
  const VkCommandBuffer& cmdBuf = m_commandBuffers[m_imageIndex];

  VkSubmitInfo         submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
  VkPipelineStageFlags stageFlags = VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_NV;
  submitInfo.pWaitDstStageMask    = &stageFlags;
  submitInfo.waitSemaphoreCount   = 1;
  submitInfo.pWaitSemaphores      = &m_semaphores.vkComplete;
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores    = &m_semaphores.vkReady;
  submitInfo.commandBufferCount   = 1;
  submitInfo.pCommandBuffers      = &cmdBuf;

  vkQueueSubmit(m_queue, 1, &submitInfo, VK_NULL_HANDLE);

  // 等待队列完成（保证输出image可读取）
  vkQueueWaitIdle(m_queue);
}

void HelloVulkan::createSemaphores()
{
  int glReadyHandle{-1};
  int glCompleteHandle{-1};

  // Vulkan
  {
    auto handleType = VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_OPAQUE_FD_BIT;
    {
      VkSemaphoreCreateInfo       sci{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
      VkExportSemaphoreCreateInfo esci{VK_STRUCTURE_TYPE_EXPORT_SEMAPHORE_CREATE_INFO};
      sci.pNext        = &esci;
      esci.handleTypes = handleType;
      vkCreateSemaphore(m_device, &sci, nullptr, &m_semaphores.vkReady);
      vkCreateSemaphore(m_device, &sci, nullptr, &m_semaphores.vkComplete);
    }
    {
      VkSemaphoreGetFdInfoKHR getFdInfo{VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR};
      getFdInfo.handleType = handleType;
      getFdInfo.semaphore  = m_semaphores.vkReady;
      vkGetSemaphoreFdKHR(m_device, &getFdInfo, &glReadyHandle);
    }
    {
      VkSemaphoreGetFdInfoKHR getFdInfo{VK_STRUCTURE_TYPE_SEMAPHORE_GET_FD_INFO_KHR};
      getFdInfo.handleType = handleType;
      getFdInfo.semaphore  = m_semaphores.vkComplete;
      vkGetSemaphoreFdKHR(m_device, &getFdInfo, &glCompleteHandle);
    }
  }

  // OpenGL
  {
    // Import semaphores
    glGenSemaphoresEXT(1, &m_semaphores.glReady);
    glGenSemaphoresEXT(1, &m_semaphores.glComplete);
    glImportSemaphoreFdEXT(m_semaphores.glReady, GL_HANDLE_TYPE_OPAQUE_FD_EXT, glReadyHandle);
    glImportSemaphoreFdEXT(m_semaphores.glComplete, GL_HANDLE_TYPE_OPAQUE_FD_EXT, glCompleteHandle);
  }
}