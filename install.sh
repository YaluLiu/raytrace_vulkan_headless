#!/bin/bash

BUILD_TYPE="Release"
DEFAULT_HYDRA_SCENE_PATH="/home/yalu/docker/assets/tile/pao/tile_pao.usd"
DEFAULT_HYDRA_SCENE_PATH=/home/yalu/docker/assets/demo5/World0.usd
DEFAULT_TRAIN_SCENE_PATH="/home/yalu/docker/assets/demo5/World0.usd"
function format(){
    find engine \( -name "*.cpp" -o -name "*.hpp" \) | xargs clang-format -i
}

function hydra(){
    set -e
    usd_path="/home/${USER}/software/USD"
    tbb_dir="${TBB_DIR:-/usr/lib/x86_64-linux-gnu/cmake/TBB}"
    plugin_names=("UsdRaySensor" "UsdRaySensorImaging" "hdRobot")
    project_root="$(pwd)"
    hydra_scene_path="${HYDRA_SCENE_PATH:-${DEFAULT_HYDRA_SCENE_PATH}}"

    mkdir -p "${project_root}/output"
    mkdir -p build

    cd build
    cmake .. -Wno-dev \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DTBB_DIR="${tbb_dir}" \
        -DCMAKE_INSTALL_PREFIX=${usd_path}/plugin/usd
    
    cmake --build . --target "${plugin_names[@]}" --config Release -j20
    for plugin_name in "${plugin_names[@]}"; do
        cmake --install . --component "${plugin_name}"
    done
    cd ..

    echo "[hydra] HYDRA_SCENE_PATH=${hydra_scene_path}"
}

function schema(){
    set -e
    usd_path="/home/${USER}/software/USD"
    tbb_dir="${TBB_DIR:-/usr/lib/x86_64-linux-gnu/cmake/TBB}"

    mkdir -p build_schema
    cd build_schema
    cmake .. -Wno-dev \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DROBOT_ENGINE_SCHEMA_ONLY=ON \
        -DTBB_DIR="${tbb_dir}" \
        -DCMAKE_INSTALL_PREFIX=${usd_path}/plugin/usd

    cmake --build . --target UsdRaySensor --config Release -j20
    cmake --install . --component UsdRaySensor
    cd ..
}

function train(){
    set -e
    tbb_dir="${TBB_DIR:-/usr/lib/x86_64-linux-gnu/cmake/TBB}"
    project_root="$(pwd)"
    train_scene_path="${TRAIN_SCENE_PATH:-${DEFAULT_TRAIN_SCENE_PATH}}"
    train_output_dir="${TRAIN_OUTPUT_DIR:-${project_root}/output/train}"
    train_frames="${TRAIN_FRAMES:-1}"
    train_width="${TRAIN_WIDTH:-1280}"
    train_height="${TRAIN_HEIGHT:-720}"

    mkdir -p "${train_output_dir}"
    mkdir -p build

    cd build
    cmake .. -Wno-dev \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DTBB_DIR="${tbb_dir}"

    cmake --build . --target robot_training_headless --config Release -j20
    cd "${project_root}"

    train_args=(
        --usd "${train_scene_path}"
        --output-dir "${train_output_dir}"
        --frames "${train_frames}"
        --width "${train_width}"
        --height "${train_height}"
        --save-preview
        --preview-lidar-points
        --preview-height-scan-points
        --export-lidar
        --export-height-scan
    )

    if [ -n "${TRAIN_CAMERA}" ]; then
        train_args+=(--camera "${TRAIN_CAMERA}")
    fi
    if [ -n "${TRAIN_PLUGIN_SEARCH_ROOT}" ]; then
        train_args+=(--plugin-search-root "${TRAIN_PLUGIN_SEARCH_ROOT}")
    fi

    echo "[train] TRAIN_SCENE_PATH=${train_scene_path}"
    echo "[train] TRAIN_OUTPUT_DIR=${train_output_dir}"
    "${project_root}/build/headlessTraining/robot_training_headless" "${train_args[@]}" "$@"
}

function train_viewer(){
    set -e
    tbb_dir="${TBB_DIR:-/usr/lib/x86_64-linux-gnu/cmake/TBB}"
    project_root="$(pwd)"
    train_scene_path="${TRAIN_SCENE_PATH:-${DEFAULT_TRAIN_SCENE_PATH}}"
    train_width="${TRAIN_WIDTH:-1280}"
    train_height="${TRAIN_HEIGHT:-720}"

    mkdir -p build

    cd build
    cmake .. -Wno-dev \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DTBB_DIR="${tbb_dir}"

    cmake --build . --target robot_training_viewer --config Release -j20
    cd "${project_root}"

    viewer_args=(
        --usd "${train_scene_path}"
        --width "${train_width}"
        --height "${train_height}"
    )

    if [ -n "${TRAIN_OUTPUT_DIR}" ]; then
        mkdir -p "${TRAIN_OUTPUT_DIR}"
        viewer_args+=(--output-dir "${TRAIN_OUTPUT_DIR}")
    fi
    if [ -n "${TRAIN_CAMERA}" ]; then
        viewer_args+=(--camera "${TRAIN_CAMERA}")
    fi
    if [ -n "${TRAIN_PLUGIN_SEARCH_ROOT}" ]; then
        viewer_args+=(--plugin-search-root "${TRAIN_PLUGIN_SEARCH_ROOT}")
    fi

    echo "[train_viewer] TRAIN_SCENE_PATH=${train_scene_path}"
    if [ -n "${TRAIN_OUTPUT_DIR}" ]; then
        echo "[train_viewer] TRAIN_OUTPUT_DIR=${TRAIN_OUTPUT_DIR}"
    fi
    "${project_root}/build/headlessTraining/robot_training_viewer" "${viewer_args[@]}" "$@"
}

function python(){
    set -e
    tbb_dir="${TBB_DIR:-/usr/lib/x86_64-linux-gnu/cmake/TBB}"
    project_root="$(pwd)"
    python_exe="${PYTHON:-python3}"
    python_exe="$("${python_exe}" -c "import sys; print(sys.executable)")"

    "${python_exe}" -c "import pybind11" 2>/dev/null || \
        "${python_exe}" -m pip install pybind11 || \
        "${python_exe}" -m pip install --user --break-system-packages pybind11

    mkdir -p build-python
    cd build-python
    cmake .. -Wno-dev \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DROBOT_ENGINE_BUILD_PYTHON=ON \
        -DPython3_EXECUTABLE="${python_exe}" \
        -DTBB_DIR="${tbb_dir}"

    cmake --build . --target robot_raster_py --config Release -j20
    cd "${project_root}"

    module_path="$(find "${project_root}/build-python/python" -maxdepth 1 -name "robot_raster_py*.so" -print -quit)"
    if [ -z "${module_path}" ]; then
        echo "[python] failed to find built robot_raster_py module"
        exit 1
    fi
    site_dir="$("${python_exe}" - <<'PY'
import os
import site
import sysconfig

platlib = sysconfig.get_path("platlib")
print(platlib if os.access(platlib, os.W_OK) else site.getusersitepackages())
PY
)"
    mkdir -p "${site_dir}"
    install -Dm755 "${module_path}" "${site_dir}/$(basename "${module_path}")"
    install -Dm755 "${project_root}/build-python/UsdRaySensor/UsdRaySensor.so" "${site_dir}/UsdRaySensor.so"

    echo "[python] installed $(basename "${module_path}") to ${site_dir}"
    echo "[python] installed UsdRaySensor.so to ${site_dir}"
    echo "[python] depth demo: ${python_exe} python/depth_camera_demo.py --usd <scene.usd>"
    echo "[python] height-scan demo: ${python_exe} python/isaac_lab_height_scan_demo.py --usd <scene.usd>"
}

function show()
{
    hydra_scene_path="${HYDRA_SCENE_PATH:-${DEFAULT_HYDRA_SCENE_PATH}}"
    /home/yalu/software/usdtweak/build/usdtweak "${hydra_scene_path}"
}

function graphify_index()
{
    set -e
    project_root="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    python3 - "${project_root}" <<'PY'
import os
import sys
from collections import Counter
from pathlib import Path

from graphify.analyze import god_nodes, surprising_connections, suggest_questions
from graphify.build import build_from_json
from graphify.cluster import cluster, score_all
from graphify.export import to_json
from graphify.extract import collect_files, extract
from graphify.report import generate

project_root = Path(sys.argv[1]).resolve()
source_roots = ("engine", "hdRobot", "headlessTraining")
missing_roots = [name for name in source_roots if not (project_root / name).is_dir()]
if missing_roots:
    raise SystemExit(f"[graphify] Missing source directories: {', '.join(missing_roots)}")

os.chdir(project_root)
code_files = []
for source_root in source_roots:
    code_files.extend(collect_files(Path(source_root)))
code_files = sorted(set(code_files))

if not code_files:
    raise SystemExit("[graphify] No code files found in the configured source directories.")

extraction = extract(code_files)
graph = build_from_json(extraction)
communities = cluster(graph)
cohesion = score_all(graph, communities)
labels = {community_id: f"Community {community_id}" for community_id in communities}
gods = god_nodes(graph)
surprises = surprising_connections(graph, communities)
questions = suggest_questions(graph, communities, labels)
detection = {
    "files": {
        "code": [str(path) for path in code_files],
        "document": [],
        "paper": [],
        "image": [],
    },
    "total_files": len(code_files),
    "total_words": 0,
}

output_dir = project_root / "graphify-out"
output_dir.mkdir(exist_ok=True)
report = generate(
    graph,
    communities,
    cohesion,
    labels,
    gods,
    surprises,
    detection,
    {"input": 0, "output": 0},
    ".",
    suggested_questions=questions,
)
(output_dir / "GRAPH_REPORT.md").write_text(report)
to_json(graph, communities, str(output_dir / "graph.json"))


def unique_anchor_labels(node_ids, limit=4):
    labels_seen = set()
    anchors = []
    ranked_nodes = sorted(
        (node_id for node_id in node_ids if node_id in graph),
        key=lambda node_id: (-graph.degree(node_id), graph.nodes[node_id].get("label", node_id)),
    )
    for node_id in ranked_nodes:
        label = graph.nodes[node_id].get("label", node_id)
        if label in labels_seen:
            continue
        labels_seen.add(label)
        anchors.append(label)
        if len(anchors) == limit:
            break
    return anchors


source_root_list = "`, `".join(source_roots)
summary_lines = [
    "# Graph Summary",
    "",
    "Generated by `bash install.sh graphify_index`. This is the default",
    "Graphify entry point; use the full report and raw graph only on demand.",
    "",
    "## Scope",
    "",
    f"- Source roots: `{source_root_list}`.",
    f"- {len(code_files)} files, {graph.number_of_nodes()} nodes, "
    f"{graph.number_of_edges()} edges, {len(communities)} communities.",
    "- Full audit: `graphify-out/GRAPH_REPORT.md`.",
    "- Raw graph: `graphify-out/graph.json` (bounded traversal only).",
    "",
    "## God Nodes",
    "",
]
summary_gods = []
seen_god_labels = set()
for god in gods:
    if god["label"] in seen_god_labels:
        continue
    seen_god_labels.add(god["label"])
    summary_gods.append(god)
    if len(summary_gods) == 8:
        break
for index, god in enumerate(summary_gods, start=1):
    summary_lines.append(f"{index}. `{god['label']}` - {god['edges']} edges")

summary_lines.extend(["", "## Largest Community Anchors", ""])
largest_communities = sorted(
    communities.items(), key=lambda item: (-len(item[1]), item[0])
)[:8]
for community_id, node_ids in largest_communities:
    roots = Counter()
    for node_id in node_ids:
        source_file = graph.nodes[node_id].get("source_file", "") if node_id in graph else ""
        if source_file:
            roots[source_file.split("/", 1)[0]] += 1
    root_names = ", ".join(root for root, _ in roots.most_common(2)) or "unknown"
    anchors = ", ".join(f"`{label}`" for label in unique_anchor_labels(node_ids))
    summary_lines.append(
        f"- Community {community_id}: {len(node_ids)} nodes; cohesion "
        f"{cohesion.get(community_id, 0.0):.2f}; roots {root_names}; anchors {anchors}."
    )

summary_lines.extend(
    [
        "",
        "## Retrieval Policy",
        "",
        "- Local file or symbol work: use CodeGraph directly; skip FileMap and",
        "  further Graphify reads when the target is already known.",
        "- Unclear ownership or entry point: use `docs/FILEMAP.md` to select one",
        "  domain map before inspecting source.",
        "- Cross-module architecture: search `GRAPH_REPORT.md` by task term or",
        "  community ID and read only the matching section.",
        "- Relationship/path questions: traverse only the relevant subgraph in",
        "  `graph.json`; never load the complete JSON into model context.",
    ]
)
(output_dir / "GRAPH_SUMMARY.md").write_text("\n".join(summary_lines) + "\n")

needs_update = output_dir / "needs_update"
if needs_update.exists():
    needs_update.unlink()

print(
    f"[graphify] Indexed {len(code_files)} files from {', '.join(source_roots)}: "
    f"{graph.number_of_nodes()} nodes, {graph.number_of_edges()} edges, "
    f"{len(communities)} communities; wrote GRAPH_SUMMARY.md"
)
PY
}

function restore()
{
    git restore .
    graphify_index
}

# function nsight{
#     sudo /opt/nvidia/nsight-graphics-for-linux/nsight-graphics-for-linux-2025.5.0.0/host/linux-desktop-nomad-x64/ngfx-ui
# }

# 动态函数调用
if [ -z "$1" ]; then
    hydra
elif declare -f "$1" > /dev/null; then
    "$1" "${@:2}"  # 调用传入的函数，并传递额外的参数
else
    echo "unknown command: $1"
    exit 1
fi
