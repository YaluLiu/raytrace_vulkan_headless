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
#if !ENABLE_HYDRA
  createOpenGLContext();
#endif
  m_allocGL.init(device, physicalDevice);
}

//--------------------------------------------------------------------------------------------------
// 每帧更新摄像机矩阵（view/proj/inverse等）到uniform buffer
void HelloVulkan::updateUniformBuffer(const VkCommandBuffer& cmdBuf)
{
  const float    aspectRatio = m_size.width / static_cast<float>(m_size.height);
  GlobalUniforms hostUBO     = {};
  const auto&    view        = CameraManip.getMatrix();
  glm::mat4      proj        = glm::perspectiveRH_ZO(glm::radians(CameraManip.getFov()), aspectRatio, 0.1f, 1000.0f);
#if !ENABLE_HYDRA
  proj[1][1] *= -1;  // Vulkan坐标系Y反转
#endif

  hostUBO.viewProj    = proj * view;
  hostUBO.viewInverse = glm::inverse(view);
  hostUBO.projInverse = glm::inverse(proj);

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

  m_rtOutputGL.destroy(m_allocGL);
  m_rtObjectIdGL.destroy(m_allocGL);
  m_rtInstanceIdGL.destroy(m_allocGL);
  m_allocGL.deinit();

  m_alloc.destroy(m_offscreenDepth);

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

  m_alloc.destroy(m_spheresBuffer);
  m_alloc.destroy(m_spheresAabbBuffer);
  m_alloc.destroy(m_spheresMatColorBuffer);
  m_alloc.destroy(m_spheresMatIndexBuffer);

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
  createInstanceIdImage();
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
    nvvk::cmdBarrierImageLayout(cmdBuf, m_offscreenInstanceId.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
    genCmdBuf.submitAndWait(cmdBuf);
  }
}