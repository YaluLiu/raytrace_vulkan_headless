#!/bin/bash

BUILD_TYPE="Release"
DEFAULT_HYDRA_SCENE_PATH="/home/yalu/docker/assets/tile/pao/tile_pao.usd"

function format(){
    find raster \( -name "*.cpp" -o -name "*.hpp" \) | xargs clang-format -i
}

function hydra(){
    set -e
    usd_path="/home/${USER}/software/USD"
    plugin_name="hdRobot"
    project_root="$(pwd)"
    hydra_scene_path="${HYDRA_SCENE_PATH:-${DEFAULT_HYDRA_SCENE_PATH}}"

    mkdir -p "${project_root}/output"

    cd build
    cmake .. -Wno-dev \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DCMAKE_INSTALL_PREFIX=${usd_path}/plugin/usd
    
    cmake --build . --target ${plugin_name} --config Release -j20
    cmake --install . --component ${plugin_name}
    cd ..

    echo "[hydra] HYDRA_SCENE_PATH=${hydra_scene_path}"
}

function show()
{
    hydra_scene_path="${HYDRA_SCENE_PATH:-${DEFAULT_HYDRA_SCENE_PATH}}"
    /home/yalu/software/usdtweak/build/usdtweak "${hydra_scene_path}"
}

function restore()
{
    git restore .
    python3 -c "from graphify.watch import _rebuild_code; from pathlib import Path; _rebuild_code(Path('.'))"
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
