from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relpath: str) -> str:
    return (ROOT / relpath).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def require_all(text: str, needles: list[str], message: str) -> None:
    missing = [needle for needle in needles if needle not in text]
    require(not missing, f"{message}: missing {missing}")


def main() -> None:
    data_loader = read("common/data_loader.h")
    model_loader = read("common/ModelLoader.h")
    scene_data_h = read("hdRobot/sceneData.h")
    scene_data_cpp = read("hdRobot/sceneData.cpp")
    material_cpp = read("hdRobot/material.cpp")
    host_device = read("headless/shaders/host_device.h")
    rchit = read("headless/shaders/raytrace.rchit")
    wavefront = read("headless/shaders/wavefront.glsl")
    cmake = read("CMakeLists.txt")

    material_fields = [
        "transmissionColorFactor",
        "subsurfaceColorFactor",
        "transmissionFactor",
        "subsurfaceFactor",
        "subsurfaceScale",
        "subsurfaceTextureId",
    ]
    require_all(data_loader, material_fields, "MaterialObj must carry advanced MaterialX fields")
    require_all(scene_data_h, material_fields, "HydraMaterial must carry advanced MaterialX fields")
    require_all(host_device, material_fields, "WaveFrontMaterial must mirror advanced MaterialX fields")

    require("Subsurface," in model_loader, "Texture registry must distinguish subsurface/scattering maps")
    require("TextureUsage::Subsurface" in scene_data_cpp, "Subsurface textures must be treated as linear data")

    require_all(
        scene_data_cpp,
        [
            "transmissionColorFactor = glm::vec3(1.0f",
            "subsurfaceColorFactor = glm::vec3(1.0f",
            "transmissionFactor = 0.0f",
            "subsurfaceFactor = 0.0f",
            "subsurfaceScale = 0.0f",
            "subsurfaceTextureId = -1",
            "mat.transmissionColorFactor = transmissionColorFactor",
            "mat.subsurfaceColorFactor = subsurfaceColorFactor",
            "mat.transmissionFactor = transmissionFactor",
            "mat.subsurfaceFactor = subsurfaceFactor",
            "mat.subsurfaceScale = subsurfaceScale",
            "mat.subsurfaceTextureId = subsurfaceTextureId",
        ],
        "Hydra material conversion must preserve advanced fields",
    )

    require_all(
        material_cpp,
        [
            'inputName == "transmission"',
            'inputName == "subsurface"',
            'paramName == "transmission"',
            'paramName == "transmission_color"',
            'paramName == "subsurface"',
            'paramName == "subsurface_color"',
            'paramName == "subsurface_scale"',
            "TextureUsage::Subsurface",
            "material.subsurfaceTextureId = textureId",
        ],
        "Material sync must parse advanced standard_surface inputs",
    )

    require_all(
        wavefront,
        [
            "float transmission;",
            "float subsurface;",
            "transmissionColor;",
            "subsurfaceColor;",
            "computeSubsurfaceWrap",
        ],
        "PBR sample helpers must expose advanced material terms",
    )
    require_all(
        rchit,
        [
            "mat.transmissionFactor",
            "mat.transmissionColorFactor",
            "mat.subsurfaceFactor",
            "mat.subsurfaceTextureId",
            "mat.subsurfaceScale",
            "computeSubsurfaceWrap",
            "mix(baseColor, mat.transmissionColorFactor",
            "pbr.diffuseAlbedo =",
        ],
        "Closest-hit shader must apply conservative transmission/subsurface approximations",
    )

    require("contracts.materialx_advanced_inputs" in cmake, "CMake must register advanced MaterialX contract")


if __name__ == "__main__":
    main()
