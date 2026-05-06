from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relpath: str) -> str:
    return (ROOT / relpath).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    material_cpp = read("hdRobot/material.cpp")
    materialx_parser_h = read("hdRobot/materialXParser.h")
    materialx_parser_cpp = read("hdRobot/materialXParser.cpp")
    cmake = read("CMakeLists.txt")

    for token in ["base_color", "metalness", "specular_roughness", "normal", "emission", "opacity"]:
        require(token in materialx_parser_cpp, f"MaterialX parser must recognize MaterialX input {token}")

    for field in [
        "baseColorTextureId",
        "metallicTextureId",
        "roughnessTextureId",
        "normalTextureId",
        "emissionTextureId",
        "opacityTextureId",
    ]:
        require(field in materialx_parser_cpp, f"MaterialX parser must populate {field}")

    require(
        "FindUpstreamTexturePath" in materialx_parser_cpp and "inputConnections" in materialx_parser_cpp,
        "MaterialX parser must walk upstream connections to locate image file inputs",
    )
    require(
        "GetResolvedPath()" in materialx_parser_cpp and "GetAssetPath()" in materialx_parser_cpp,
        "MaterialX parser must preserve a usable asset path even when the resolver has no resolved path",
    )
    require("ParseMaterialXNetwork" in materialx_parser_h, "MaterialX parser must expose ParseMaterialXNetwork")
    require("ApplyMaterialXTextureId" in materialx_parser_h, "MaterialX parser must expose texture id application")
    require("ParseMaterialXNetwork(network" in material_cpp, "HdRobotMaterial must call ParseMaterialXNetwork")
    require("contracts.materialx_standard_surface" in cmake, "CMake must register the MaterialX standard_surface contract")


if __name__ == "__main__":
    main()
