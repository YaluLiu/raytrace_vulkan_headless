#pragma once

#include "hello_vulkan.hpp"
#include <vector>
#include <string>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "nvpsystem.hpp"
#include "nvvk/context_vk.hpp"

class RayTraceApp
{
public:
  RayTraceApp();
  ~RayTraceApp();

  // init vulkan and hello-vulkan
  void setup(int width = 1280, int height = 720);

  // 渲染
  void resize(int w, int h);

  // 加载OBJ模型
  void loadScene();

  // 创建光追结构
  void createBVH();

  // 更新模型的mesh和translation
  void animation();

  // 渲染
  void render();

  // 清理资源
  void cleanup();

  // save local png file
  void saveFrame(std::string outputImagePath = "headless.png");

  // 
  GLuint getOpenGLFrame() { return m_helloVk.getOpenGLFrame(); }
  HelloVulkan& getVulkan() { return m_helloVk; };

private:
  int m_width  = 1280;
  int m_height = 720;

  HelloVulkan m_helloVk;

  // Vulkan context
  nvvk::Context m_vkctx;

  // for init 
  void setupCamera();
  void UpdateCamera();

  void setupContext();
  void setupHelloVulkan();

  // for compute animation,test on the Specified model file
  std::chrono::system_clock::time_point m_startTime;

  bool _cleaned = false;
};