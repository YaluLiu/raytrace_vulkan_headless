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
    nvvk::cmdBarrierImageLayout(cmdBuf, m_offscreenObjectId.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    genCmdBuf.submitAndWait(cmdBuf);
  }
}

void HelloVulkan::initRayTracing()
{
  VkPhysicalDeviceProperties2 prop2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
  prop2.pNext = &m_rtProperties;
  vkGetPhysicalDeviceProperties2(m_physicalDevice, &prop2);

  m_rtBuilder.setup(m_device, &m_alloc, m_graphicsQueueIndex);
  m_sbtWrapper.setup(m_device, m_graphicsQueueIndex, &m_alloc, m_rtProperties);
}

//--------------------------------------------------------------------------------------------------
// 将一个OBJ模型转为Vulkan光追BLAS所需的Geometry结构
// 返回：nvvk::RaytracingBuilderKHR::BlasInput
auto HelloVulkan::objectToVkGeometryKHR(const ObjModel& model)
{
  VkDeviceAddress vertexAddress = nvvk::getBufferDeviceAddress(m_device, model.vertexBuffer.buffer);
  VkDeviceAddress indexAddress  = nvvk::getBufferDeviceAddress(m_device, model.indexBuffer.buffer);

  uint32_t maxPrimitiveCount = model.nbIndices / 3;

  VkAccelerationStructureGeometryTrianglesDataKHR triangles{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR};
  triangles.vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT;  // 顶点为vec3
  triangles.vertexData.deviceAddress = vertexAddress;
  triangles.vertexStride             = sizeof(VertexObj);
  triangles.indexType                = VK_INDEX_TYPE_UINT32;
  triangles.indexData.deviceAddress  = indexAddress;
  triangles.maxVertex                = model.nbVertices - 1;

  VkAccelerationStructureGeometryKHR asGeom{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR};
  asGeom.geometryType       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  asGeom.flags              = VK_GEOMETRY_OPAQUE_BIT_KHR;
  asGeom.geometry.triangles = triangles;

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

void HelloVulkan::createBottomLevelAS()
{
  m_blas.reserve(m_objModel.size());
  for(const auto& obj : m_objModel)
  {
    auto blas = objectToVkGeometryKHR(obj);
    m_blas.push_back(blas);
  }
  m_rtBuilder.buildBlas(m_blas, VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR
                                    | VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_BUILD_BIT_KHR);
}

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

  m_rtFlags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
  m_rtBuilder.buildTlas(m_tlas, m_rtFlags);
}