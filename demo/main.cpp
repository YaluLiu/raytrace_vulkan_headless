#include "ray_trace_app.hpp"
#include "obj_loader.h"
#include "usd_loader.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <utility>
#include "nvh/cameramanipulator.hpp"
#include "nvgl/contextwindow_gl.hpp"

namespace fs = std::filesystem;

namespace {
constexpr int   kRenderWidth            = 3840;
constexpr int   kRenderHeight           = 2160;
constexpr int   kOutputFrameCount       = 10;
constexpr float kMainCameraStepDeg      = 30.0f;
const fs::path  kOutputDirectory        = "result";
// y-height z-front
const glm::vec3 kInitialCameraEye       = {1.2f, 2.5f, 10.0f};
const glm::vec3 kInitialLidarEye        = {0.2f, 1.0f, 8.0f};
const glm::vec3 kInitialCameraCenter    = {1.0f, 1.0f, 0.6f};
const glm::vec3 kInitialCameraUp        = {0.0f, 1.0f, 0.0f};
const glm::vec3 kGroundPlaneScale       = {2.f, 1.f, 2.f};
const glm::vec3 kIdentityScale          = {1.f, 1.f, 1.f};
const glm::vec3 kCatTranslation         = {0.0f, 0.5f, 0.0f};
const glm::vec3 kBeautyBallTranslation  = {2.0f, 0.5f, 1.0f};
const LidarParams kDefaultRadarParams   = {-120.0f, 120.0f, 1.0f, -2.0f, -20.0f, 0.5f, 2.0f, 200.0f};

class DemoOpenGLContext
{
public:
  DemoOpenGLContext()
  {
    if(glfwInit() == GLFW_FALSE)
    {
      throw std::runtime_error("Failed to initialize GLFW for demo OpenGL context");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    m_window = glfwCreateWindow(1, 1, "raytrace-demo-hidden-gl-context", nullptr, nullptr);
    if(m_window == nullptr)
    {
      glfwTerminate();
      throw std::runtime_error("Failed to create hidden GLFW window for demo OpenGL context");
    }

    glfwMakeContextCurrent(m_window);
    load_GL(nvgl::ContextWindow::sysGetProcAddress);
  }

  ~DemoOpenGLContext()
  {
    if(m_window != nullptr)
    {
      glfwDestroyWindow(m_window);
      m_window = nullptr;
    }
    glfwTerminate();
  }

  DemoOpenGLContext(const DemoOpenGLContext&)            = delete;
  DemoOpenGLContext& operator=(const DemoOpenGLContext&) = delete;

private:
  GLFWwindow* m_window{nullptr};
};

void setEnvDefault(const char* name, const char* value)
{
  if(std::getenv(name) == nullptr)
  {
    setenv(name, value, 0);
  }
}

void applyDefaultDlssRuntime()
{
  static constexpr std::array<std::pair<const char*, const char*>, 3> kDefaults = {{
      {"ENABLE_DLSS_RR", "1"},
      {"ENABLE_DLSS_SR", "1"},
      {"DLSS_SR_SCALE", "1"},
  }};

  for(const auto& [name, value] : kDefaults)
  {
    setEnvDefault(name, value);
  }
}

std::string buildOutputFramePath(const fs::path& outputDir, int frameIndex)
{
  return (outputDir / (std::to_string(frameIndex) + ".png")).string();
}

glm::vec3 rotateAroundYAxis(const glm::vec3& offset, float angleDeg)
{
  const float angleRad = glm::radians(angleDeg);
  const float cosTheta = std::cos(angleRad);
  const float sinTheta = std::sin(angleRad);
  return {cosTheta * offset.x + sinTheta * offset.z, offset.y, -sinTheta * offset.x + cosTheta * offset.z};
}
}  // namespace

class RayTraceAppTest
{
public:
  explicit RayTraceAppTest(bool useUsd = true)
      : m_testOnUsd(useUsd)
      , m_app()
  {
    m_cwd = fs::current_path();
  }

  void run()
  {
    applyDefaultDlssRuntime();

    m_app.setup(kRenderWidth, kRenderHeight);
    DemoOpenGLContext glContext;

    loadConfiguredScene();
    addDefaultLight();
    setupInitialCamera();
    renderFrames();
  }

  void addDefaultLight()
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
  void loadConfiguredScene()
  {
    if(m_testOnUsd)
    {
      loadUsdScene();
      return;
    }
    loadObjScene();
  }

  void setupInitialCamera()
  {
    // 相机贴近主体（猫+球），让目标占据画面更大面积
    CameraManip.setLookat(kInitialCameraEye,    // eye
                          kInitialCameraCenter, // ctr: cat(0,0.5,0) 与 ball(2,0.5,1) 的中间区域
                          kInitialCameraUp);    // up
  }

  void renderFrames()
  {
    fs::create_directories(kOutputDirectory);
    setFixedRadarCameraFromMainCamera();
    m_app.render();

    for(int frameIndex = 0; frameIndex < kOutputFrameCount; ++frameIndex)
    {
      updateMainCameraForFrame(frameIndex);
      m_app.render();
      m_app.saveFrame(buildOutputFramePath(kOutputDirectory, frameIndex));
    }
  }

  void updateMainCameraForFrame(int frameIndex)
  {
    const glm::vec3 orbitOffset = rotateAroundYAxis(kInitialCameraEye - kInitialCameraCenter,
                                                    static_cast<float>(frameIndex) * kMainCameraStepDeg);
    CameraManip.setLookat(kInitialCameraCenter + orbitOffset, kInitialCameraCenter, kInitialCameraUp);
  }

  void setFixedRadarCameraFromMainCamera()
  {
    RayTraceApp::RadarCameraInput radarCamera{};
    radarCamera.eye         = kInitialLidarEye;
    radarCamera.center      = CameraManip.getCenter();
    radarCamera.up          = CameraManip.getUp();
    radarCamera.fovDeg      = CameraManip.getFov();
    radarCamera.lidarParams = {kDefaultRadarParams.azimuthMinDeg, kDefaultRadarParams.azimuthMaxDeg,
                               kDefaultRadarParams.azimuthStepDeg, kDefaultRadarParams.verticalMinDeg,
                               kDefaultRadarParams.verticalMaxDeg, kDefaultRadarParams.verticalStepDeg,
                               kDefaultRadarParams.pointRadiusPixels, kDefaultRadarParams.maxDistance};
    m_app.setRadarCamera(radarCamera);
  }

  void loadGroundPlane()
  {
    ObjLoader planeLoader;
    planeLoader.loadModel(m_cwd / "media/scenes/plane.obj");
    m_app.getVulkan().loadModel(planeLoader, glm::scale(glm::mat4(1.f), kGroundPlaneScale));
  }

  void loadObjScene()
  {
    // 球体
    ObjLoader sphereLoader;
    sphereLoader.loadModel(m_cwd / "media/scenes/sphere.obj");
    m_app.getVulkan().loadModel(sphereLoader);

    loadGroundPlane();
    m_app.createBVH();
  }

  void loadUsdScene()
  {
    // cat
    UsdLoader catLoader;
    catLoader.loadModel(m_cwd / "media/scenes/cat/cat.usdz");

    catLoader.m_textures.clear();
    catLoader.m_textures.push_back("cat.png");
    catLoader.m_textures.push_back("beautyball.png");  // only support load once on start
    catLoader.m_materials[0].textureID = 0;

    m_app.getVulkan().loadModel(catLoader, glm::scale(glm::mat4(1.f), kIdentityScale)
                                               * glm::translate(glm::mat4(1.0f), kCatTranslation));

    UsdLoader ballLoader;
    ballLoader.loadModel(m_cwd / "media/scenes/beautyball/beautyball.usdz");
    ballLoader.m_textures.clear();
    ballLoader.m_materials[0].textureID = 1;

    m_app.getVulkan().loadModel(ballLoader, glm::scale(glm::mat4(1.f), kIdentityScale)
                                                * glm::translate(glm::mat4(1.0f), kBeautyBallTranslation));

    loadGroundPlane();

    m_app.createBVH();
  }

  fs::path                              m_cwd;
  bool                                  m_testOnUsd;
  RayTraceApp                           m_app;
};

int main()
{
  RayTraceAppTest test(/*useUsd=*/true);  // 或 false
  test.run();
  return 0;
}
