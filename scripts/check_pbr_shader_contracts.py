from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relpath: str) -> str:
    return (ROOT / relpath).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    rchit = read("headless/shaders/raytrace.rchit")
    wavefront = read("headless/shaders/wavefront.glsl")
    cmake = read("CMakeLists.txt")

    for symbol in [
        "PbrMaterialSample",
        "samplePbrMaterial",
        "computePbrDirectLighting",
        "computeMetallicRoughnessSpecularF0",
    ]:
        require(symbol in rchit + wavefront, f"PBR shader path must define/use {symbol}")

    for texture_id in ["baseColorTextureId", "metallicTextureId", "roughnessTextureId", "emissionTextureId"]:
        require(texture_id in rchit, f"raytrace.rchit must sample {texture_id}")

    require("mat.textureId" in rchit and "legacy" in rchit.lower(), "raytrace.rchit must keep legacy diffuse texture fallback")
    require("mat.baseColorFactor" in rchit, "PBR base color must use baseColorFactor")
    require("mat.metallicFactor" in rchit, "PBR metallic must use metallicFactor")
    require("mat.roughnessFactor" in rchit, "PBR roughness must use roughnessFactor")

    require("prd.firstHitDiffuseValid" in rchit and "pbr.diffuseAlbedo" in rchit, "DLSS diffuse guide must use PBR diffuse albedo")
    require("prd.firstHitSpecularPad" in rchit and "pbr.specularF0" in rchit, "DLSS specular guide must use PBR F0")
    require("prd.firstHitWorldPosRoughness" in rchit and "pbr.roughness" in rchit, "DLSS roughness guide must use PBR roughness")

    require("roughnessFromShininess(mat.shininess)" not in rchit, "raytrace.rchit must not drive roughness from Phong shininess")
    require("computeDirectLighting(" not in rchit, "raytrace.rchit must use PBR direct lighting helper")
    require("contracts.pbr_shader" in cmake, "CMake must register the PBR shader contract")


if __name__ == "__main__":
    main()
