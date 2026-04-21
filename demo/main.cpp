#include "ray_trace_app.hpp"
#include "obj_loader.h"
#include "usd_loader.h"
#include <filesystem>
#include <iostream>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include "nvh/cameramanipulator.hpp"

namespace fs = std::filesystem;

namespace {
void setEnvDefault(const char* name, const char* value)
{
  if(std::getenv(name) == nullptr)
  {
    setenv(name, value, 0);
  }
}

void applyDefaultDlssRuntime()
{
  setEnvDefault("ENABLE_DLSS_RR", "1");
  setEnvDefault("ENABLE_DLSS_SR", "1");
  setEnvDefault("DLSS_SR_SCALE", "1");
}
}  // namespace

class RayTraceAppTest
{
public:
  RayTraceAppTest(bool useUsd = true)
      : m_testOnUsd(useUsd)
      , m_startTime(std::chrono::system_clock::now())
      , m_app()
  {
    m_cwd = fs::current_path();
  }

  void run()
  {
    applyDefaultDlssRuntime();

    int width  = 3840;
    int height = 2160;
    m_app.setup(width, height);

    if(m_testOnUsd)
    {
      loadUsdScene();
    }
    else
    {
      loadObjScene();
    }
    add_light();

    fs::create_directories("result");

    if(!m_testOnUsd)
    {
      animate();
      m_app.render();
      m_app.saveFrame("result/0.png");
      return;
    }

    // 相机贴近主体（猫+球），让目标占据画面更大面积
    CameraManip.setLookat(glm::vec3(0.2f, 1.35f, 3.6f),   // eye
                          glm::vec3(1.0f, 1.0f, 0.6f),    // ctr: cat(0,0.5,0) 与 ball(2,0.5,1) 的中间区域
                          glm::vec3(0.0f, 1.0f, 0.0f));   // up

    m_app.render();
    for(int i = 0; i < 1; ++i)
    {
      updatecamera();
      m_app.render();
      m_app.saveFrame((fs::path("result") / (std::to_string(i) + ".png")).string());
    }
  }

  void updatecamera()
  {
    float     radius = 3.2f;  // 更近的轨道半径，让主体显著放大
    glm::vec3 ctr    = CameraManip.getCenter();
    glm::vec3 up     = CameraManip.getUp();

    // 每次调用让角度递增
    m_yaw += 0.2f;  // 控制转动速度

    // 假设 up 是 (0, 1, 0)，在水平面围绕 ctr 旋转
    float x = ctr.x + radius * cos(m_yaw);
    float z = ctr.z + radius * sin(m_yaw);
    float y = ctr.y + 0.45f;

    glm::vec3 eye(x, y, z);

    CameraManip.setLookat(eye, ctr, up);

    ++m_cameraUpdateCount;
    if(m_cameraUpdateCount <= 10)
    {
      std::cout << std::fixed << std::setprecision(6)
                << "[camera] update#" << m_cameraUpdateCount << " yaw=" << m_yaw << " eye=(" << eye.x << ", " << eye.y
                << ", " << eye.z << ") ctr=(" << ctr.x << ", " << ctr.y << ", " << ctr.z << ") up=(" << up.x << ", "
                << up.y << ", " << up.z << ")\n";
    }
  }

  void add_light()
  {
    m_app.getVulkan().clearLights();
    Light default_light;
    default_light.type         = 0;  //sphere
    default_light.baseEmission = {800.0f, 800.0f, 800.0f};
    default_light.diffuse      = 1.0;
    default_light.specular     = 1.0;
    default_light.radius       = 2;

    default_light.position = {0.0f, 5.0f, 30.0f};
    m_app.getVulkan().addLight(default_light);
  }

private:
  void loadObjScene()
  {
    // 平面
    ObjLoader planeLoader;
    planeLoader.loadModel(m_cwd / "media/scenes/plane.obj");
    m_app.getVulkan().loadModel(planeLoader, glm::scale(glm::mat4(1.f), glm::vec3(2.f, 1.f, 2.f)));

    // wuson
    ObjLoader wusonLoader;
    wusonLoader.loadModel(m_cwd / "media/scenes/wuson.obj");
    m_app.getVulkan().loadModel(wusonLoader);

    // 多个wuson实例
    uint32_t  wusonId = 1;
    glm::mat4 identity{1};
    for(int i = 0; i < 5; i++)
    {
      m_app.getVulkan().m_instances.push_back({identity, wusonId});
    }

    // 球体
    ObjLoader sphereLoader;
    sphereLoader.loadModel(m_cwd / "media/scenes/sphere.obj");
    m_app.getVulkan().loadModel(sphereLoader);
    m_app.createBVH();
  }

  void loadUsdScene()
  {
    // cat
    UsdLoader catloader;
    catloader.loadModel(m_cwd / "media/scenes/cat/cat.usdz");

    catloader.m_textures.clear();
    catloader.m_textures.push_back("cat.png");
    catloader.m_textures.push_back("beautyball.png"); //only support load once on start
    catloader.m_materials[0].textureID = 0;

    m_app.getVulkan().loadModel(catloader, glm::scale(glm::mat4(1.f), glm::vec3(1.f, 1.f, 1.f))
                                               * glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f)));


    UsdLoader ballloader;
    ballloader.loadModel(m_cwd / "media/scenes/beautyball/beautyball.usdz");
    ballloader.m_textures.clear();
    ballloader.m_materials[0].textureID = 1;

    m_app.getVulkan().loadModel(ballloader, glm::scale(glm::mat4(1.f), glm::vec3(1.f, 1.f, 1.f))
                                                * glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, 0.5f, 1.0f)));

    // 平面,只有obj的平面
    ObjLoader planeLoader;
    planeLoader.loadModel("media/scenes/plane.obj");
    m_app.getVulkan().loadModel(planeLoader, glm::scale(glm::mat4(1.f), glm::vec3(2.f, 1.f, 2.f)));

    m_app.createBVH();
  }

  void loadScene()
  {
    m_app.loadScene();
    m_app.createBVH();
  }

  void animate()
  {
    std::chrono::duration<float> diff = std::chrono::system_clock::now() - m_startTime;
    m_app.getVulkan().animationObject(diff.count());
    m_app.getVulkan().animationInstances(diff.count());
  }

  fs::path                              m_cwd;
  bool                                  m_testOnUsd;
  std::chrono::system_clock::time_point m_startTime;
  RayTraceApp                           m_app;
  float                                 m_yaw = 1.6f;  // 首次 updatecamera() 后 yaw=1.6，对齐你指定的 update#4 参数
  int                                   m_cameraUpdateCount = 0;
};

int main()
{
  RayTraceAppTest test(/*useUsd=*/true);  // 或 false
  test.run();
  return 0;
}
