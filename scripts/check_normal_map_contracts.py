from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relpath: str) -> str:
    return (ROOT / relpath).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    data_loader_h = read("common/data_loader.h")
    scene_data_h = read("hdRobot/sceneData.h")
    scene_data_cpp = read("hdRobot/sceneData.cpp")
    mesh_cpp = read("hdRobot/mesh.cpp")
    host_device_h = read("headless/shaders/host_device.h")
    rchit = read("headless/shaders/raytrace.rchit")
    hello_material_cpp = read("headless/hello_vulkan_material.cpp")
    cmake = read("CMakeLists.txt")

    require("glm::vec4 tangent" in data_loader_h, "VertexObj must carry tangent xyz plus bitangent sign")
    require("vec4 tangent" in host_device_h, "GPU Vertex must carry tangent xyz plus bitangent sign")
    require("VtVec3fArray tangents" in scene_data_h, "HydraMesh must carry tangents")
    require("VtFloatArray bitangentSigns" in scene_data_h, "HydraMesh must carry bitangent signs")
    require("mesh.tangents" in scene_data_cpp and "mesh.bitangentSigns" in scene_data_cpp, "ConvertVmeshToLoader must read tangent arrays")
    require("vertex.tangent" in scene_data_cpp, "ConvertVmeshToLoader must write VertexObj tangent")
    require("sceneMesh.tangents" in mesh_cpp and "sceneMesh.bitangentSigns" in mesh_cpp, "HdRobotMesh must store tangents in HydraMesh")
    require("_CalculateTangents(" in mesh_cpp, "HdRobotMesh must calculate fallback tangents")

    for symbol in ["sampleNormalMap", "mat.normalTextureId", "normalSample", "tangentW", "bitangent"]:
        require(symbol in rchit, f"raytrace.rchit must use {symbol} for normal mapping")

    require("objectToWorldNormal" in rchit, "normal map tangents must be transformed to world space")
    require("mat3(gl_ObjectToWorldEXT)" in rchit, "normal map tangents must follow instance transforms")
    require("vec4(objectToWorldNormal * tangentObj.xyz, tangentObj.w)" in rchit, "bitangent sign must survive tangent transform")
    require("worldNrm = sampleNormalMap" in rchit, "normal map must replace shading normal before lighting")
    require("offsetof(VertexObj, tangent) == offsetof(Vertex, tangent)" in hello_material_cpp, "Vertex tangent layout must be asserted")
    require("contracts.normal_map" in cmake, "CMake must register the normal map contract")


if __name__ == "__main__":
    main()
