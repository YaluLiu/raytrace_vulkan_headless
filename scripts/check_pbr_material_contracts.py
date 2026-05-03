from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relpath: str) -> str:
    return (ROOT / relpath).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def require_all(text: str, needles: list[str], message: str) -> None:
    missing = [needle for needle in needles if needle not in text]
    require(not missing, f"{message}: missing {', '.join(missing)}")


def main() -> None:
    data_loader_h = read("common/data_loader.h")
    scene_data_h = read("hdRobot/sceneData.h")
    scene_data_cpp = read("hdRobot/sceneData.cpp")
    bridge_cpp = read("hdRobot/headlessRenderBridge.cpp")
    host_device_h = read("headless/shaders/host_device.h")
    cmake = read("CMakeLists.txt")

    pbr_factor_fields = [
        "baseColorFactor",
        "metallicFactor",
        "roughnessFactor",
        "emissionFactor",
        "opacityFactor",
    ]
    pbr_texture_fields = [
        "baseColorTextureId",
        "metallicTextureId",
        "roughnessTextureId",
        "normalTextureId",
        "emissionTextureId",
        "opacityTextureId",
    ]

    require_all(data_loader_h, pbr_factor_fields + pbr_texture_fields, "MaterialObj must carry PBR factors and texture ids")
    require_all(scene_data_h, pbr_factor_fields + pbr_texture_fields, "HydraMaterial must carry PBR factors and texture ids")
    require_all(host_device_h, pbr_factor_fields + pbr_texture_fields, "WaveFrontMaterial must expose PBR fields to shaders")

    require("mat.textureID" in scene_data_cpp and "= textureID;" in scene_data_cpp, "Legacy textureID mapping must remain intact")
    require_all(scene_data_cpp, [f"mat.{field}" for field in pbr_factor_fields + pbr_texture_fields], "toMaterialObj must copy PBR fields")
    require_all(bridge_cpp, [f"result.{field}" for field in pbr_factor_fields + pbr_texture_fields], "Hydra bridge must copy PBR fields")

    require("contracts.pbr_material" in cmake, "CMake must register the PBR material contract")


if __name__ == "__main__":
    main()
