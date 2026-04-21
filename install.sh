#!/bin/bash

BUILD_TYPE="Release"
app_name="headless"

function vulkan(){
    # 设置版本和目录
    VULKAN_VERSION="1.4.328.1"
    VULKAN_SDK_DIR="$HOME/test/vulkan-sdk"
    VULKAN_ARCHIVE="vulkansdk-linux-x86_64-${VULKAN_VERSION}.tar.xz"
    
    echo "开始安装 Vulkan SDK ${VULKAN_VERSION}..."
    
    # 创建安装目录
    mkdir -p "$VULKAN_SDK_DIR"
    cd "$VULKAN_SDK_DIR" || exit 1
    
    # 下载 Vulkan SDK
    echo "正在下载 Vulkan SDK..."
    wget https://sdk.lunarg.com/sdk/download/${VULKAN_VERSION}/linux/${VULKAN_ARCHIVE}
    
    # 检查下载是否成功
    if [ $? -ne 0 ]; then
        echo "下载失败！"
        return 1
    fi
    
    # 解压文件
    echo "正在解压..."
    tar -xf ${VULKAN_ARCHIVE}
    
    # 删除压缩包
    rm ${VULKAN_ARCHIVE}

    # 设置环境变量脚本路径
    SETUP_ENV_SCRIPT="${VULKAN_SDK_DIR}/${VULKAN_VERSION}/setup-env.sh"

    echo ""
    echo "Vulkan SDK 安装完成！"
    echo "请将以下内容添加到你的 ~/.bashrc 或 ~/.zshrc 文件中："
    echo ""
    echo "source \"${SETUP_ENV_SCRIPT}\""
    echo ""
    echo "然后运行: source ~/.bashrc (或 source ~/.zshrc)"
    echo "然后运行: vulkaninfo,验证是否安装成功"
}

function format(){
    find headless -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i
}

function demo() {
    set -e
    project_root="$(pwd)"
    dlss_sdk_root="${DLSS_SDK_ROOT:-/home/yalu/dlss/DLSS}"
    dlss_feature_path="${DLSS_RR_FEATURE_PATH:-${dlss_sdk_root}/lib/Linux_x86_64/rel}"
    dlss_appdata_path="${DLSS_RR_APPDATA_PATH:-${project_root}/output/ngx}"
    dlss_status_log="${DLSS_RR_STATUS_LOG:-${project_root}/output/dlss_rr_demo_status.log}"

    if [ ! -f "${dlss_sdk_root}/include/nvsdk_ngx_vk.h" ] || [ ! -f "${dlss_sdk_root}/lib/Linux_x86_64/libnvsdk_ngx.a" ]; then
        echo "[demo] DLSS SDK is incomplete: ${dlss_sdk_root}"
        echo "[demo] required files:"
        echo "       ${dlss_sdk_root}/include/nvsdk_ngx_vk.h"
        echo "       ${dlss_sdk_root}/lib/Linux_x86_64/libnvsdk_ngx.a"
        return 1
    fi

    if [ ! -d "${dlss_feature_path}" ]; then
        echo "[demo] warning: DLSS runtime feature path does not exist: ${dlss_feature_path}"
    fi

    mkdir -p "${project_root}/output" "${dlss_appdata_path}" "${project_root}/result"
    : > "${dlss_status_log}"

    cd build
    cmake ..  \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DENABLE_GL_VK_CONVERSION=ON \
        -DENABLE_HYDRA=OFF \
        -DENABLE_DLSS_RR=ON \
        -DDLSS_SDK_ROOT="${dlss_sdk_root}"
    make -j20
    cd ..

    export ENABLE_DLSS_RR="${ENABLE_DLSS_RR:-1}"
    export ENABLE_DLSS_SR="${ENABLE_DLSS_SR:-1}"
    export DLSS_SR_SCALE="${DLSS_SR_SCALE:-0.6}"
    export DLSS_RR_FEATURE_PATH="${dlss_feature_path}"
    export DLSS_RR_APPDATA_PATH="${dlss_appdata_path}"
    export DLSS_RR_STATUS_LOG="${dlss_status_log}"
    export DLSS_SDK_ROOT="${dlss_sdk_root}"
    export __NGX_LOG_PATH_OVERRIDE="${dlss_appdata_path}"
    export __NGX_LOG_LEVEL="${__NGX_LOG_LEVEL:-0}"

    find "${project_root}/result" -maxdepth 1 -type f -name "gl_*.png" -delete

    echo "[demo] ENABLE_DLSS_RR=${ENABLE_DLSS_RR}"
    echo "[demo] ENABLE_DLSS_SR=${ENABLE_DLSS_SR}"
    echo "[demo] DLSS_SR_SCALE=${DLSS_SR_SCALE}"
    echo "[demo] DLSS_RR_FEATURE_PATH=${DLSS_RR_FEATURE_PATH}"
    echo "[demo] DLSS_RR_APPDATA_PATH=${DLSS_RR_APPDATA_PATH}"
    echo "[demo] DLSS_RR_STATUS_LOG=${DLSS_RR_STATUS_LOG}"

    build/bin/${BUILD_TYPE}/vk_${app_name}_KHR_app
    # build/bin/${BUILD_TYPE}/libheadless_app
}

function anim() {
    app_name="ray_tracing_animation"
    set -e
    cd build
    cmake ..  \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DENABLE_GL_VK_CONVERSION=OFF \
        -DENABLE_HYDRA=OFF
    make -j20
    cd ..
    build/bin/${BUILD_TYPE}/vk_${app_name}_KHR_app
}

function hydra(){
    set -e
    usd_path="/home/${USER}/software/USD"
    plugin_name="hdRobot"
    project_root="$(pwd)"
    dlss_sdk_root="${DLSS_SDK_ROOT:-/home/yalu/dlss/DLSS}"
    dlss_feature_path="${DLSS_RR_FEATURE_PATH:-${dlss_sdk_root}/lib/Linux_x86_64/rel}"
    dlss_appdata_path="${DLSS_RR_APPDATA_PATH:-${project_root}/output/ngx}"
    dlss_status_log="${DLSS_RR_STATUS_LOG:-${project_root}/output/dlss_rr_status.log}"

    if [ ! -f "${dlss_sdk_root}/include/nvsdk_ngx_vk.h" ] || [ ! -f "${dlss_sdk_root}/lib/Linux_x86_64/libnvsdk_ngx.a" ]; then
        echo "[hydra] DLSS SDK is incomplete: ${dlss_sdk_root}"
        echo "[hydra] required files:"
        echo "        ${dlss_sdk_root}/include/nvsdk_ngx_vk.h"
        echo "        ${dlss_sdk_root}/lib/Linux_x86_64/libnvsdk_ngx.a"
        return 1
    fi

    if [ ! -d "${dlss_feature_path}" ]; then
        echo "[hydra] warning: DLSS runtime feature path does not exist: ${dlss_feature_path}"
    fi

    mkdir -p "${project_root}/output" "${dlss_appdata_path}"
    : > "${dlss_status_log}"

    cd build
    cmake .. -Wno-dev \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DENABLE_GL_VK_CONVERSION=ON \
        -DENABLE_HYDRA=ON \
        -DENABLE_DLSS_RR=ON \
        -DDLSS_SDK_ROOT="${dlss_sdk_root}" \
        -DCMAKE_INSTALL_PREFIX=${usd_path}/plugin/usd
    
    cmake --build . --target ${plugin_name} --config Release -j20
    cmake --install . --component ${plugin_name}
    cd ..

    export ENABLE_DLSS_RR="${ENABLE_DLSS_RR:-1}"
    export RT_SPP="${RT_SPP:-4}"
    export DLSS_RR_FEATURE_PATH="${dlss_feature_path}"
    export DLSS_RR_APPDATA_PATH="${dlss_appdata_path}"
    export DLSS_RR_STATUS_LOG="${dlss_status_log}"
    export __NGX_LOG_PATH_OVERRIDE="${dlss_appdata_path}"
    export __NGX_LOG_LEVEL="${__NGX_LOG_LEVEL:-0}"

    echo "[hydra] ENABLE_DLSS_RR=${ENABLE_DLSS_RR}"
    echo "[hydra] RT_SPP=${RT_SPP}"
    echo "[hydra] DLSS_RR_FEATURE_PATH=${DLSS_RR_FEATURE_PATH}"
    echo "[hydra] DLSS_RR_APPDATA_PATH=${DLSS_RR_APPDATA_PATH}"
    echo "[hydra] DLSS_RR_STATUS_LOG=${DLSS_RR_STATUS_LOG}"

    /home/yalu/software/usdtweak/build/usdtweak /home/yalu/docker/assets/unit_test/anim/pao/pao.usd
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
if declare -f "$1" > /dev/null; then
    "$1" "${@:2}"  # 调用传入的函数，并传递额外的参数
else
    hydra
fi
