import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relpath: str) -> str:
    return (ROOT / relpath).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    host_device = read("headless/shaders/host_device.h")
    lidar_composite = read("headless/shaders/lidar_composite.comp")
    hello_hpp = read("headless/hello_vulkan.hpp")
    hello_cpp = read("headless/hello_vulkan.cpp")
    lidar_cpp = read("headless/hello_vulkan_lidar.cpp")
    pipeline_cpp = read("headless/hello_vulkan_pipeline.cpp")
    app_cpp = read("headless/ray_trace_app.cpp")
    render_texture_export_cpp = read("hdRobot/renderTextureExport.cpp")
    hello_test_cpp = read("headless/hello_vulkan_test.cpp")

    require("START_BINDING(LidarCompositeBindings)" in host_device, "lidar composite pass must have descriptor bindings")
    require("eCompositeDenoisedImage" in host_device, "lidar composite must bind the denoised final image")
    require("eCompositeLidarPointCloudImage" in host_device, "lidar composite must bind the raw lidar point-cloud image")
    require("eCompositeDepthImage" in host_device, "lidar composite must bind the normalized depth AOV image")
    require("eCompositeLidarDepthKeyImage" in host_device, "lidar composite must bind lidar depth keys")
    require(
        "layout(set = 0, binding = eCompositeDenoisedImage, rgba32f) uniform image2D denoisedImage;" in lidar_composite,
        "lidar composite shader must bind the denoised final image",
    )
    require(
        "renderLidarPointCloud(const VkCommandBuffer& cmdBuf);" in hello_hpp,
        "HelloVulkan must expose a post-DLSS lidar generation pass",
    )
    require(
        "compositeLidar(const VkCommandBuffer& cmdBuf);" in hello_hpp,
        "HelloVulkan must expose a post-DLSS lidar composite pass",
    )
    dlss_call = "m_helloVk.runDlssRR(cmdBuf);"
    lidar_call = "m_helloVk.renderLidarPointCloud(cmdBuf);"
    composite_call = "m_helloVk.compositeLidar(cmdBuf);"
    require(dlss_call in app_cpp and lidar_call in app_cpp and composite_call in app_cpp,
            "RayTraceApp::render must run DLSS, lidar point generation, and lidar composite")
    require(
        app_cpp.index(dlss_call) < app_cpp.index(lidar_call),
        "DLSS must run before lidar point generation",
    )
    require(
        app_cpp.index(lidar_call) < app_cpp.index(composite_call),
        "lidar generation must run before final composite",
    )
    require(
        re.search(r"lowResPassPc\.lidarPassMode\s*=\s*eRaygenPassLowResBeauty\s*;", pipeline_cpp) is not None,
        "scene raytrace must not pre-compose lidar into raw color",
    )
    require("imageStore(denoisedImage" in lidar_composite, "post-DLSS composite must write into final output")
    require(
        "imageLoad(depthImage" in lidar_composite and "kLidarDepthCompositeEpsilon" in lidar_composite,
        "post-DLSS composite must preserve depth occlusion",
    )
    require(
        "ivec2 lidarPixel" in lidar_composite and "imageSize(lidarPointCloudDepthKeyImage)" in lidar_composite,
        "post-DLSS composite must map final pixels to scene-depth pixels",
    )
    require(
        re.search(r"groupX\s*=\s*\(m_size\.width\s*\+\s*kLocalSize\s*-\s*1\)\s*/\s*kLocalSize\s*;", lidar_cpp) is not None
        and re.search(r"groupY\s*=\s*\(m_size\.height\s*\+\s*kLocalSize\s*-\s*1\)\s*/\s*kLocalSize\s*;", lidar_cpp) is not None
        and re.search(r"vkCmdDispatch\(\s*cmdBuf\s*,\s*groupX\s*,\s*groupY\s*,\s*1\s*\)\s*;", lidar_cpp) is not None,
        "post-DLSS composite must dispatch over the final output extent",
    )
    require(
        "m_offscreenLidarPointCloudFormat" in hello_cpp
        and "kInteropUsage | VK_IMAGE_USAGE_TRANSFER_DST_BIT" in hello_cpp
        and "&m_rtLidarPointCloudGL, GL_RGBA32F, GL_NEAREST" in hello_cpp
        and "GL_NEAREST, m_aovSize" in hello_cpp,
        "raw lidar point-cloud AOV must be full-resolution and clearable",
    )
    require(
        "name == HdRobotAovTokens->lidarPointCloud" in render_texture_export_cpp
        and "app.m_rtLidarPointCloudGL.oglId" in render_texture_export_cpp,
        "Hydra lidar:pointCloud AOV must continue to expose the raw point-cloud image",
    )
    require(
        "void HelloVulkan::dumpLidarInteropTexture" in hello_test_cpp and "m_rtLidarPointCloudGL.oglId" in hello_test_cpp,
        "standalone lidar PNG dump must continue to read the raw point-cloud image",
    )
    require(
        "m_offscreenDenoised.descriptor.imageView" in lidar_cpp,
        "final image descriptor must target m_offscreenDenoised",
    )


if __name__ == "__main__":
    main()
