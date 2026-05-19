#pragma once

#include "raster_renderer.hpp"
#include <vector>
#include <string>
#include <chrono>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include "nvpsystem.hpp"
#include "nvvk/context_vk.hpp"

class RasterSession
{
public:
  RasterSession();
  ~RasterSession();

  void setup(int width = 1280, int height = 720);
  void setPluginSearchRoot(std::string pluginSearchRoot);

  void resize(int w, int h);

  void createRenderResources();

  void render();

  void cleanup();

  void saveFrame(std::string outputImagePath = "headless.png");

  RasterRenderer& getRenderer() { return m_renderer; };

private:
  int m_width  = 1280;
  int m_height = 720;

  RasterRenderer m_renderer;

  nvvk::Context m_vkctx;

  void setupCamera();

  void setupContext();
  void setupRenderer();

  std::chrono::system_clock::time_point m_startTime;

  bool _cleaned = false;
  bool m_rendererCreated = false;
  bool m_resourcesCreated = false;
  std::string m_pluginSearchRoot;
};
