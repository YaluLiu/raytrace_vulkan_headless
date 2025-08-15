#include "ray_trace_app.hpp"
#include "obj_loader.h"
#include "usd_loader.h"
#include <filesystem>
#include <iostream>
#include <chrono>
#include "nvh/cameramanipulator.hpp"

namespace fs = std::filesystem;

class RayTraceAppTest {
public:
    RayTraceAppTest(bool useUsd = true)
        : m_testOnUsd(useUsd),
          m_startTime(std::chrono::system_clock::now()),
          m_app()
    {
        m_cwd = fs::current_path();
    }

    void run()
    {
        int width = 1280;
        int height = 720;
        m_app.setup(width, height);

        if (m_testOnUsd) {
            loadUsdScene();
        } else {
            loadObjScene();
        }

        for (int i = 0; i < 10; i++) {
            // 测试窗口resize
            if (i % 3 == 0) {
                width = static_cast<int>(width * 1.1);
                height = static_cast<int>(height * 1.1);
            }
            m_app.resize(width, height);

            if (!m_testOnUsd) {
                animate();
            }
            updatecamera();
            m_app.render();

            std::string pngname = "result/" + std::to_string(i) + ".png";
            m_app.saveFrame(pngname);
        }
    }

    void updatecamera()
    { 
      float radius = 10.0f; // 距离目标点的半径，可根据需要调整
      glm::vec3 ctr = CameraManip.getCenter();
      glm::vec3 up = CameraManip.getUp();

      // 每次调用让角度递增
      m_yaw += 0.2f; // 控制转动速度

      // 假设 up 是 (0, 1, 0)，在水平面围绕 ctr 旋转
      float x = ctr.x + radius * cos(m_yaw);
      float z = ctr.z + radius * sin(m_yaw);
      float y = ctr.y + 0.0f; // 可以固定高度，也可以做垂直旋转

      glm::vec3 eye(x, y, z);

      CameraManip.setLookat(eye, ctr, up);
    }

private:
    void loadObjScene()
    {
        // 平面
        ObjLoader planeLoader;
        planeLoader.loadModel(m_cwd / "media/scenes/plane.obj");
        m_app.getVulkan().loadModel(planeLoader,
            glm::scale(glm::mat4(1.f), glm::vec3(2.f, 1.f, 2.f)));

        // wuson
        ObjLoader wusonLoader;
        wusonLoader.loadModel(m_cwd / "media/scenes/wuson.obj");
        m_app.getVulkan().loadModel(wusonLoader);

        // 多个wuson实例
        uint32_t wusonId = 1;
        glm::mat4 identity{1};
        for (int i = 0; i < 5; i++) {
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

        // todo: change usd texture file path
        catloader.m_textures.clear();
        catloader.m_textures.push_back("texture_pbr_v128.png");

        m_app.getVulkan().loadModel(catloader,
            glm::scale(glm::mat4(1.f), glm::vec3(4.f, 4.f, 4.f)) *
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.5f, 0.0f)));

        // 平面,只有obj的平面
        ObjLoader planeLoader;
        planeLoader.loadModel("media/scenes/plane.obj");
        m_app.getVulkan().loadModel(planeLoader,
            glm::scale(glm::mat4(1.f), glm::vec3(2.f, 1.f, 2.f)));

        m_app.createBVH();
    }

    void animate()
    {
        std::chrono::duration<float> diff = std::chrono::system_clock::now() - m_startTime;
        m_app.getVulkan().animationObject(diff.count());
        m_app.getVulkan().animationInstances(diff.count());
    }

    fs::path m_cwd;
    bool m_testOnUsd;
    std::chrono::system_clock::time_point m_startTime;
    RayTraceApp m_app;
    float m_yaw = 0.0f; // 在文件顶部或类成员变量中定义
};

int main()
{
    RayTraceAppTest test(/*useUsd=*/true); // 或 false
    test.run();
    return 0;
}