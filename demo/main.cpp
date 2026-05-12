#include "ray_trace_app.hpp"
#include "obj_loader.h"
#include "usd_loader.h"
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include "nvh/cameramanipulator.hpp"

namespace fs = std::filesystem;

namespace {
constexpr int   kRenderWidth            = 3840;
constexpr int   kRenderHeight           = 2160;
constexpr int   kOutputFrameCount       = 10;
constexpr float kMainCameraStepDeg      = 30.0f;
const fs::path  kOutputDirectory        = "result";
// y-height z-front
const glm::vec3 kInitialCameraEye       = {1.2f, 2.5f, 10.0f};
const glm::vec3 kInitialCameraCenter    = {1.0f, 1.0f, 0.6f};
const glm::vec3 kInitialCameraUp        = {0.0f, 1.0f, 0.0f};
const glm::vec3 kGroundPlaneScale       = {2.f, 1.f, 2.f};
const glm::vec3 kIdentityScale          = {1.f, 1.f, 1.f};
const glm::vec3 kCatTranslation         = {0.0f, 0.5f, 0.0f};
const glm::vec3 kBeautyBallTranslation  = {2.0f, 0.5f, 1.0f};

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

bool hasRenderableGeometry(const ModelLoader& loader)
{
  return !loader.m_vertices.empty() && !loader.m_indices.empty();
}

void setFirstMaterialTexture(ModelLoader& loader, int textureId)
{
  if(loader.m_materials.empty())
  {
    return;
  }
  loader.m_materials[0].diffuseTextureId = textureId;
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
    m_app.setup(kRenderWidth, kRenderHeight);

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

  void loadGroundPlane()
  {
    ObjLoader planeLoader;
    planeLoader.loadModel((m_cwd / "media/scenes/plane.obj").string());
    m_app.getVulkan().loadModel(planeLoader, glm::scale(glm::mat4(1.f), kGroundPlaneScale));
  }

  void loadObjScene()
  {
    // 球体
    ObjLoader sphereLoader;
    sphereLoader.loadModel((m_cwd / "media/scenes/sphere.obj").string());
    m_app.getVulkan().loadModel(sphereLoader);

    loadGroundPlane();
    m_app.createRenderResources();
  }

  void loadBeautyBallFallback()
  {
    ObjLoader sphereLoader;
    const fs::path spherePath = m_cwd / "media/scenes/sphere.obj";
    sphereLoader.loadModel(spherePath.string());
    if(!hasRenderableGeometry(sphereLoader))
    {
      throw std::runtime_error("Failed to load fallback sphere asset: " + spherePath.string());
    }

    m_app.getVulkan().loadModel(sphereLoader, glm::scale(glm::mat4(1.f), kIdentityScale)
                                                  * glm::translate(glm::mat4(1.0f), kBeautyBallTranslation));
  }

  void loadUsdScene()
  {
    // cat
    UsdLoader catLoader;
    const fs::path catPath = m_cwd / "media/scenes/cat/cat.usdz";
    catLoader.loadModel(catPath.string());
    if(!hasRenderableGeometry(catLoader))
    {
      throw std::runtime_error("Failed to load required cat USD asset: " + catPath.string());
    }

    catLoader.m_textures.clear();
    catLoader.m_textures.push_back("asset_0.jpg");
    setFirstMaterialTexture(catLoader, 0);

    m_app.getVulkan().loadModel(catLoader, glm::scale(glm::mat4(1.f), kIdentityScale)
                                               * glm::translate(glm::mat4(1.0f), kCatTranslation));

    const fs::path beautyBallPath = m_cwd / "media/scenes/beautyball/beautyball.usdz";
    if(fs::exists(beautyBallPath))
    {
      UsdLoader ballLoader;
      ballLoader.loadModel(beautyBallPath.string());
      if(hasRenderableGeometry(ballLoader))
      {
        ballLoader.m_textures.clear();
        ballLoader.m_textures.push_back("asset_1.jpg");
        setFirstMaterialTexture(ballLoader, 0);

        m_app.getVulkan().loadModel(ballLoader, glm::scale(glm::mat4(1.f), kIdentityScale)
                                                    * glm::translate(glm::mat4(1.0f), kBeautyBallTranslation));
      }
      else
      {
        std::cerr << "Failed to load beautyball USD, using sphere fallback: " << beautyBallPath << std::endl;
        loadBeautyBallFallback();
      }
    }
    else
    {
      std::cerr << "Missing beautyball USD, using sphere fallback: " << beautyBallPath << std::endl;
      loadBeautyBallFallback();
    }

    loadGroundPlane();

    m_app.createRenderResources();
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
