from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relpath: str) -> str:
    return (ROOT / relpath).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    model_loader_h = read("common/ModelLoader.h")
    render_texture_export_h = read("hdRobot/renderTextureExport.h")
    render_texture_export_cpp = read("hdRobot/renderTextureExport.cpp")
    bridge_cpp = read("hdRobot/headlessRenderBridge.cpp")
    hello_hpp = read("headless/hello_vulkan.hpp")
    hello_material_cpp = read("headless/hello_vulkan_material.cpp")
    cmake = read("CMakeLists.txt")

    require("struct TextureAsset" in model_loader_h, "ModelLoader must expose an in-memory texture asset payload")
    require(
        "std::vector<TextureAsset> m_textureAssets" in model_loader_h,
        "ModelLoader must carry encoded texture assets separately from legacy filename textures",
    )
    require(
        "std::vector<TextureAsset> ExportRegisteredTextures" in render_texture_export_h,
        "Hydra texture export must return texture assets, not exported filenames",
    )
    require(
        "asset->GetBuffer()" in render_texture_export_cpp and "encodedBytes" in render_texture_export_cpp,
        "Hydra texture export must copy resolved asset bytes into memory",
    )
    require(
        "std::ofstream" not in render_texture_export_cpp and "media/textures/" not in render_texture_export_cpp,
        "Hydra texture export must not write material textures to media/textures",
    )
    require(
        'EnsureDirectoryExists("media/textures")' not in bridge_cpp,
        "HeadlessRenderBridge must not create the texture staging directory",
    )
    require(
        "loader.m_textureAssets" in bridge_cpp
        and "ExportRegisteredTextures(_renderParam.GetTextureAssets())" in bridge_cpp,
        "HeadlessRenderBridge must pass encoded Hydra textures through ModelLoader",
    )
    require(
        "createTextureImages(" in hello_hpp
        and "const std::vector<std::string>& textures" in hello_hpp
        and "const std::vector<TextureAsset>& textureAssets" in hello_hpp,
        "HelloVulkan texture upload API must accept legacy filenames plus encoded texture assets",
    )
    require(
        "stbi_load_from_memory" in hello_material_cpp,
        "HelloVulkan must decode Hydra texture assets from memory",
    )
    require(
        "media/textures/" in hello_material_cpp and "stbi_load(" in hello_material_cpp,
        "Legacy OBJ filename texture loading must remain supported",
    )
    require(
        "contracts.material_texture_in_memory" in cmake,
        "CMake must register the in-memory material texture contract",
    )


if __name__ == "__main__":
    main()
