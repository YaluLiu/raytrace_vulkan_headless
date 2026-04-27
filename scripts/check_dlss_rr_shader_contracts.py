from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relpath: str) -> str:
    return (ROOT / relpath).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    host_device = read("headless/shaders/host_device.h")
    raygen = read("headless/shaders/raytrace.rgen")
    rchit = read("headless/shaders/raytrace.rchit")
    rchit2 = read("headless/shaders/raytrace2.rchit")
    rmiss = read("headless/shaders/raytrace.rmiss")
    raycommon = read("headless/shaders/raycommon.glsl")
    hello_cpp = read("headless/hello_vulkan.cpp")
    pipeline_cpp = read("headless/hello_vulkan_pipeline.cpp")
    hello_hpp = read("headless/hello_vulkan.hpp")

    require("float       jitterX;" in host_device, "PushConstantRay must expose per-frame jitterX")
    require("float       jitterY;" in host_device, "PushConstantRay must expose per-frame jitterY")
    require("m_currentJitter" in hello_hpp, "HelloVulkan must retain the jitter used by the traced frame")
    require("computeDlssJitter" in pipeline_cpp, "raytrace() must compute deterministic per-frame jitter")
    require("m_pcRay.jitterX" in pipeline_cpp, "raytrace() must push jitterX to raygen")
    require("m_pcRay.jitterY" in pipeline_cpp, "raytrace() must push jitterY to raygen")
    require("eval.jitterX" in hello_cpp and "-m_currentJitter.x" in hello_cpp,
            "DLSS evaluate must receive negative shader jitterX")
    require("eval.jitterY" in hello_cpp and "-m_currentJitter.y" in hello_cpp,
            "DLSS evaluate must receive negative shader jitterY")

    require("cameraJitter" in raygen and "vec2(pcRay.jitterX, pcRay.jitterY)" in raygen,
            "raygen must use the pushed per-frame jitter")
    require("pixelCenter + cameraJitter" in raygen, "camera rays must use the stable per-frame jitter")
    require("vec2 jitter = vec2(rand01(seed), rand01(seed))" not in raygen,
            "raygen must not use random per-sample camera jitter")
    require("currPixel    = pixelCenter + cameraJitter" in raygen,
            "motion vectors must subtract the same jittered current pixel used by raygen")
    require("currPixel    = vec2(pixel) + vec2(0.5)" not in raygen,
            "motion vectors must not subtract an unjittered current pixel")

    require("normalizeViewZ" not in raygen, "linear DLSS depth must not be normalized")
    require("linearDepth      = viewZ;" in raygen, "primary-hit DLSS depth must be linear viewZ")
    require("linearDepth        = kDlssInfDistance;" in raygen, "sky DLSS depth must use kDlssInfDistance")

    require("const float kDlssInfDistance" in raycommon, "DLSS infinity distance must be shared by raygen/miss/hit")
    require("recordSpecularHitDistance" in raycommon, "shared helper must record secondary specular hit distance")
    require("recordSpecularHitDistance(prd, gl_HitTEXT);" in rchit,
            "triangle closest-hit must record first secondary specular hit distance")
    require("recordSpecularHitDistance(prd, gl_HitTEXT);" in rchit2,
            "procedural closest-hit must record first secondary specular hit distance")
    require("recordSpecularMissDistance(prd);" in rmiss,
            "miss shader must preserve infinity semantics for specular rays that hit sky")


if __name__ == "__main__":
    main()
