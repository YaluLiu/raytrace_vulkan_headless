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
    materialx_parser_cpp = read("hdRobot/materialXParser.cpp")
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

    require("MaterialInputRule" in materialx_parser_cpp, "MaterialX parser must declare texture usage in input rules")
    require("AddTextureBinding" in materialx_parser_cpp, "MaterialX parser must centralize supported texture bindings")
    require("AddUnsupportedTextureBinding" in materialx_parser_cpp, "MaterialX parser must record unsupported textures")
    require("unsupportedTextures" in materialx_parser_cpp, "MaterialX parser must expose unsupported texture records")
    for usage in ["BaseColor", "Metallic", "Roughness", "Normal", "Emission", "Opacity", "Subsurface"]:
        require(f"TextureUsage::{usage}" in materialx_parser_cpp, f"{usage} textures must be assigned by parser rules")
    require(
        "rule.textureUsage" in materialx_parser_cpp and "MaterialSemantic::Unsupported" in materialx_parser_cpp,
        "Parser must route texture usage from rules and avoid misbinding unsupported inputs",
    )
    require(
        "sourceOutput" in materialx_parser_cpp and "channel" in materialx_parser_cpp and "inputName" in materialx_parser_cpp,
        "Texture bindings must preserve usage context and channel metadata",
    )

    require("VK_FORMAT_R8G8B8A8_SRGB" in hello_material_cpp, "Texture upload must still support sRGB")
    require("VK_FORMAT_R8G8B8A8_UNORM" in hello_material_cpp, "Texture upload must support linear UNORM")
    require("textureAsset.colorSpace" in hello_material_cpp, "Texture upload must choose format from color space metadata")

    require("contracts.texture_usage" in cmake, "CMake must register the texture usage contract")


if __name__ == "__main__":
    main()
