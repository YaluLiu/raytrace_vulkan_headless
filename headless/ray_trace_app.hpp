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

  void setup(int width = 1280, int height = 720);
  void setPluginSearchRoot(std::string pluginSearchRoot);

  void resize(int w, int h);

  void createRenderResources();

  void render();

  void cleanup();

  void saveFrame(std::string outputImagePath = "headless.png");

  HelloVulkan& getVulkan() { return m_helloVk; };

private:
  int m_width  = 1280;
  int m_height = 720;

  HelloVulkan m_helloVk;

  nvvk::Context m_vkctx;

  void setupCamera();
  void UpdateCamera();

  void setupContext();
  void setupHelloVulkan();

  std::chrono::system_clock::time_point m_startTime;

  bool _cleaned = false;
  bool m_helloCreated = false;
  bool m_resourcesCreated = false;
  std::string m_pluginSearchRoot;
};
