#include "ray_trace_app.hpp"
#include "obj_loader.h"
#include "usd_loader.h"
#include <filesystem>
#include <iostream>

std::chrono::system_clock::time_point m_startTime = std::chrono::system_clock::now();

namespace fs = std::filesystem;

// 获取当前工作目录
fs::path cwd = fs::current_path();

void loadObjScene(RayTraceApp& app)
{
  // 平面
  ObjLoader planeLoader;
  planeLoader.loadModel(cwd / "media/scenes/plane.obj");
  app.getVulkan().loadModel(planeLoader,
                      glm::scale(glm::mat4(1.f), glm::vec3(2.f, 1.f, 2.f)));

  // wuson
  ObjLoader wusonLoader;
  wusonLoader.loadModel(cwd / "media/scenes/wuson.obj");
  app.getVulkan().loadModel(wusonLoader);

  // 多个wuson实例
  uint32_t  wusonId = 1;
  glm::mat4 identity{1};
  for(int i = 0; i < 5; i++)
  {
    app.getVulkan().m_instances.push_back({identity, wusonId});
  }

  // 球体
  ObjLoader sphereLoader;
  sphereLoader.loadModel(cwd / "media/scenes/sphere.obj");
  app.getVulkan().loadModel(sphereLoader);


  app.createBVH();
}

void loadUsdScene(RayTraceApp& app)
{
  // 平面
  UsdLoader catloader;
  catloader.loadModel(cwd / "media/scenes/cat/cat.usdz");
  app.getVulkan().loadModel(catloader,
                      glm::scale(glm::mat4(1.f), glm::vec3(1.f, 1.f, 1.f)));


  // // 平面,只有obj的平面
  // ObjLoader planeLoader;
  // planeLoader.loadModel("media/scenes/plane.obj");
  // app.getVulkan().loadModel(planeLoader,
  //                     glm::scale(glm::mat4(1.f), glm::vec3(2.f, 1.f, 2.f)));

  app.createBVH();
}

void animation(RayTraceApp& app)
{
  std::chrono::duration<float> diff = std::chrono::system_clock::now() - m_startTime;
  app.getVulkan().animationObject(diff.count());
  app.getVulkan().animationInstances(diff.count());
}


void test_obj()
{
  int width = 1280;
  int height = 720;
  RayTraceApp app;
  app.setup(width, height);
  loadObjScene(app);
  for(int i = 0; i < 10; i++)
  {
    //test for resize 
    if(i % 3 == 0){
      width *= 1.1;
      height *= 1.1;
    }
    app.resize(width, height);

    // test for animation
    animation(app);

    // real work
    app.render();

    // save local png file
    app.saveFrame();
  }
}

void test_usd()
{
  int width = 1280;
  int height = 720;
  RayTraceApp app;
  app.setup(width, height);
  loadUsdScene(app);
  for(int i = 0; i < 10; i++)
  {
    //test for resize 
    if(i % 3 == 0){
      width *= 1.1;
      height *= 1.1;
    }
    app.resize(width, height);

    // real work
    app.render();

    // save local png file
    app.saveFrame();
  }
}

int main()
{
  test_usd();
}