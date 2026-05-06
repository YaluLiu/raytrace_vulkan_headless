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

    require("ParseMaterialXNetwork(network" in material_cpp, "HdRobotMaterial must call ParseMaterialXNetwork")
    require(
        "base_color" not in material_cpp and "specular_roughness" not in material_cpp,
        "HdRobotMaterial must not own MaterialX input-name parsing",
    )

    require("enum class ShaderFamily" in materialx_parser_cpp, "MaterialX parser must classify shader families")
    require("MaterialXStandardSurface" in materialx_parser_cpp, "Parser must support MaterialX standard_surface")
    require("MaterialXOpenPbrSurface" in materialx_parser_cpp, "Parser must support OpenPBR surface")
    require("ND_standard_surface_surfaceshader" in materialx_parser_cpp, "Parser must recognize standard_surface ids")
    require("open_pbr_surface" in materialx_parser_cpp, "Parser must recognize OpenPBR shader ids")
    require("SurfaceShaderCandidate" in materialx_parser_cpp, "Parser must model surface shader candidates")
    require("SelectSurfaceShaderCandidate" in materialx_parser_cpp, "Parser must select a single surface shader")
    require('"mtlx:surface"' in materialx_parser_cpp, "Parser must prefer the MaterialX surface terminal")

    for token in ["base_color", "metalness", "specular_roughness", "normal", "emission", "opacity"]:
        require(token in materialx_parser_cpp, f"MaterialX parser rules must recognize MaterialX input {token}")

    for token in ["base_metalness", "base_diffuse_roughness", "geometry_normal", "emission_luminance"]:
        require(token in materialx_parser_cpp, f"OpenPBR parser rules must recognize input {token}")

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
        "ResolveUpstreamTexture" in materialx_parser_cpp and "inputConnections" in materialx_parser_cpp,
        "MaterialX parser must walk upstream connections to locate image file inputs",
    )
    require("kMaxMaterialXTraversalDepth" in materialx_parser_cpp, "Upstream traversal must have a named depth limit")
    require("std::set<SdfPath>" in materialx_parser_cpp, "Upstream traversal must guard against graph cycles")
    require("sourceOutput" in materialx_parser_h, "Texture bindings must preserve upstream source output")
    require("channel" in materialx_parser_h, "Texture bindings must preserve packed channel metadata")
    require(
        "GetResolvedPath()" in materialx_parser_cpp and "GetAssetPath()" in materialx_parser_cpp,
        "MaterialX parser must preserve a usable asset path even when the resolver has no resolved path",
    )
    require("ParseMaterialXNetwork" in materialx_parser_h, "MaterialX parser must expose ParseMaterialXNetwork")
    require("ApplyMaterialXTextureId" in materialx_parser_h, "MaterialX parser must expose texture id application")
    require("contracts.materialx_standard_surface" in cmake, "CMake must register the MaterialX standard_surface contract")


if __name__ == "__main__":
    main()
