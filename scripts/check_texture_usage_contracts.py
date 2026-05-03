from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[1]


def read(relpath: str) -> str:
    return (ROOT / relpath).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    model_loader_h = read("common/ModelLoader.h")
    scene_data_h = read("hdRobot/sceneData.h")
    scene_data_cpp = read("hdRobot/sceneData.cpp")
    render_param_h = read("hdRobot/renderParam.h")
    render_param_cpp = read("hdRobot/renderParam.cpp")
    render_texture_export_h = read("hdRobot/renderTextureExport.h")
    render_texture_export_cpp = read("hdRobot/renderTextureExport.cpp")
    material_cpp = read("hdRobot/material.cpp")
    hello_material_cpp = read("headless/hello_vulkan_material.cpp")
    bridge_cpp = read("hdRobot/headlessRenderBridge.cpp")
    cmake = read("CMakeLists.txt")

    require("enum class TextureUsage" in model_loader_h, "ModelLoader must define texture usage metadata")
    for usage in ["BaseColor", "Metallic", "Roughness", "Normal", "Emission", "Opacity"]:
        require(usage in model_loader_h, f"TextureUsage must include {usage}")
    require("enum class TextureColorSpace" in model_loader_h, "ModelLoader must define texture color space metadata")
    require(re.search(r"TextureUsage\s+usage", model_loader_h) is not None, "TextureAsset must carry usage")
    require(re.search(r"TextureColorSpace\s+colorSpace", model_loader_h) is not None, "TextureAsset must carry color space")

    require("TextureColorSpaceForUsage" in scene_data_h + scene_data_cpp, "TextureRegistry must derive color space from usage")
    require(
        "Register(const std::string" in scene_data_h and "TextureUsage usage" in scene_data_h,
        "TextureRegistry must register path plus usage",
    )
    require("GetTextureAssets()" in scene_data_h + scene_data_cpp, "TextureRegistry must expose texture metadata")
    require(
        "RegisterTexturePath(const std::string" in render_param_h and "TextureUsage usage" in render_param_h,
        "RenderParam must accept texture usage",
    )
    require("textureRegistry.Register(texturePath, usage)" in render_param_cpp, "RenderParam must forward texture usage")

    require(
        "ExportRegisteredTextures(const std::vector<TextureAsset>" in render_texture_export_h,
        "Hydra texture export must preserve registered texture metadata",
    )
    require("textureAsset.usage" in render_texture_export_cpp and "textureAsset.colorSpace" in render_texture_export_cpp, "Export must preserve usage and color space")
    require("ExportRegisteredTextures(_renderParam.GetTextureAssets())" in bridge_cpp, "Bridge must export texture metadata, not paths only")

    require("TextureUsage::BaseColor" in material_cpp, "Base color textures must be registered as BaseColor")
    require("TextureUsage::Metallic" in material_cpp, "Metallic textures must be registered as Metallic")
    require("TextureUsage::Roughness" in material_cpp, "Roughness textures must be registered as Roughness")
    require("TextureUsage::Normal" in material_cpp, "Normal textures must be registered as Normal")

    require("VK_FORMAT_R8G8B8A8_SRGB" in hello_material_cpp, "Texture upload must still support sRGB")
    require("VK_FORMAT_R8G8B8A8_UNORM" in hello_material_cpp, "Texture upload must support linear UNORM")
    require("textureAsset.colorSpace" in hello_material_cpp, "Texture upload must choose format from color space metadata")

    require("contracts.texture_usage" in cmake, "CMake must register the texture usage contract")


if __name__ == "__main__":
    main()
