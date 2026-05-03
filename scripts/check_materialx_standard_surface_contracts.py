from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relpath: str) -> str:
    return (ROOT / relpath).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    material_cpp = read("hdRobot/material.cpp")
    cmake = read("CMakeLists.txt")

    for token in ["base_color", "metalness", "specular_roughness", "normal", "emission", "opacity"]:
        require(token in material_cpp, f"HdRobotMaterial must recognize MaterialX input {token}")

    for field in [
        "baseColorTextureId",
        "metallicTextureId",
        "roughnessTextureId",
        "normalTextureId",
        "emissionTextureId",
        "opacityTextureId",
    ]:
        require(field in material_cpp, f"HdRobotMaterial must populate {field}")

    require(
        "FindUpstreamTexturePath" in material_cpp and "inputConnections" in material_cpp,
        "HdRobotMaterial must walk upstream connections to locate image file inputs",
    )
    require(
        "GetResolvedPath()" in material_cpp and "GetAssetPath()" in material_cpp,
        "HdRobotMaterial must preserve a usable asset path even when the resolver has no resolved path",
    )
    require("contracts.materialx_standard_surface" in cmake, "CMake must register the MaterialX standard_surface contract")


if __name__ == "__main__":
    main()
