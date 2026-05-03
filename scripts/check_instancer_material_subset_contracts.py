from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def read(relpath: str) -> str:
    return (ROOT / relpath).read_text(encoding="utf-8")


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def main() -> None:
    mesh_cpp = read("hdRobot/mesh.cpp")
    instancer_cpp = read("hdRobot/instancer.cpp")
    scene_data_cpp = read("hdRobot/sceneData.cpp")
    bridge_cpp = read("hdRobot/headlessRenderBridge.cpp")
    cmake = read("CMakeLists.txt")

    require("topology.GetGeomSubsets()" in mesh_cpp, "Mesh sync must read Hydra geom subsets")
    require("sceneMesh.scene_mat_ids.clear()" in mesh_cpp, "Mesh sync must rebuild local-to-global material map")
    require("sceneMesh.scene_mat_ids.emplace_back(0)" in mesh_cpp, "Mesh sync must keep a fallback material slot")
    require("sceneMesh.scene_mat_ids.emplace_back(materialPrim->_mat_id)" in mesh_cpp, "Geom subsets must append material ids")
    require("materialIds[triIdx] = localMatIdx" in mesh_cpp, "Geom subset faces must map to local material indices")
    require("loader.m_matIndx.push_back" in scene_data_cpp, "Hydra material ids must be copied into ModelLoader")
    require("for (auto &matId : curMesh.scene_mat_ids)" in bridge_cpp, "Bridge must upload local material table per mesh")
    require("for (size_t localMatIdx = 0; localMatIdx < curMesh.scene_mat_ids.size(); ++localMatIdx)" in bridge_cpp, "Runtime material updates must use local material indices")

    require("ComputeFlattenedTransforms" in instancer_cpp, "PointInstancer transforms must be flattened")
    require("GetInstanceIndices(id, prototypeId)" in instancer_cpp, "Instancer must respect prototype instance indices")
    require("curMesh.instanceTransforms.size()" in bridge_cpp, "Bridge must size TLAS slots from authored instances")
    require("vulkan.addInstance(instance.transform, instance.objIndex" in bridge_cpp, "Bridge must add TLAS instances for PointInstancer entries")
    require("curMesh.tlasIds.push_back" in bridge_cpp, "Bridge must remember per-mesh TLAS ids")
    require(
        "instanceId < curMesh.instanceTransforms.size()" in bridge_cpp and "instanceValid" in bridge_cpp,
        "TLAS update must guard instance transform bounds",
    )
    require("curMesh.transform * curMesh.instanceTransforms[instanceId]" in bridge_cpp, "TLAS update must combine mesh and instance transforms")

    require("contracts.instancer_material_subset" in cmake, "CMake must register the instancer/material subset contract")


if __name__ == "__main__":
    main()
