from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relpath: str) -> str:
    return (ROOT / relpath).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    bridge_h = read("hdRobot/headlessRenderBridge.h")
    bridge_cpp = read("hdRobot/headlessRenderBridge.cpp")
    render_pass_h = read("hdRobot/renderPass.h")
    render_pass_cpp = read("hdRobot/renderPass.cpp")
    ray_trace_app_cpp = read("headless/ray_trace_app.cpp")
    cmake = read("hdRobot/CMakeLists.txt")

    require("class HeadlessRenderBridge" in bridge_h, "HeadlessRenderBridge must be declared in its own header")
    require("RayTraceApp" in bridge_h, "HeadlessRenderBridge must own the RayTraceApp integration boundary")
    require("RenderFrame(" in bridge_h, "HeadlessRenderBridge must expose a single frame execution entry point")
    require("HeadlessRenderBridge::RenderFrame" in bridge_cpp, "Bridge frame execution must live outside renderPass.cpp")
    require("CopyAovToRenderBuffer" in bridge_cpp, "Bridge must own copying renderer AOVs back into Hydra buffers")
    require("MarkAllMeshesTlasDirty" in bridge_cpp, "Bridge must own render-tag driven TLAS invalidation")
    require("headlessRenderBridge.cpp" in cmake, "hdRobot target must compile the bridge implementation")
    require('"headlessRenderBridge.h"' not in render_pass_h, "HdRobotRenderPass header must not include bridge internals")
    require("class HeadlessRenderBridge;" in render_pass_h, "HdRobotRenderPass must forward-declare the bridge")
    require("std::unique_ptr<HeadlessRenderBridge> _bridge;" in render_pass_h,
            "HdRobotRenderPass must own the bridge behind an incomplete-type pointer")
    require('"headlessRenderBridge.h"' in render_pass_cpp, "HdRobotRenderPass implementation must include the bridge header")
    require("RayTraceApp" not in render_pass_h, "HdRobotRenderPass must not expose RayTraceApp directly")
    require("app_update" not in render_pass_h and "app_init_or_resize" not in render_pass_h,
            "Headless app helper declarations must move out of HdRobotRenderPass")
    require("_bridge->RenderFrame(renderPassState, renderTags)" in render_pass_cpp,
            "HdRobotRenderPass::_Execute must delegate frame execution to the bridge")
    require("enum class HeadlessRenderPass" in ray_trace_app_cpp,
            "RayTraceApp must name each backend render pass explicitly")
    require("kHeadlessRenderPassSequence" in ray_trace_app_cpp,
            "RayTraceApp must keep the backend pass order in a single sequence")
    require("executeHeadlessRenderPass" in ray_trace_app_cpp,
            "RayTraceApp must execute named backend passes through a dispatcher")
    require("for(const HeadlessRenderPass pass : kHeadlessRenderPassSequence)" in ray_trace_app_cpp,
            "RayTraceApp::render must iterate the explicit backend pass sequence")
    require("HeadlessRenderPass::DlssResolve" in ray_trace_app_cpp
            and "HeadlessRenderPass::LidarPointCloud" in ray_trace_app_cpp
            and "HeadlessRenderPass::LidarComposite" in ray_trace_app_cpp,
            "DLSS and LiDAR stages must be visible in the backend pass sequence")


if __name__ == "__main__":
    main()
