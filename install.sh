#!/bin/bash

BUILD_TYPE="Release"
app_name="headless"
DEFAULT_HYDRA_SCENE_PATH="/home/yalu/docker/assets/unit_test/anim/pao/pao.usd"

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

    mkdir -p "${project_root}/output" "${project_root}/result"

    cd build
    cmake ..  \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DENABLE_GL_VK_CONVERSION=OFF \
        -DENABLE_HYDRA=OFF
    make -j20
    cd ..

    find "${project_root}/result" -maxdepth 1 -type f -name "gl_*.png" -delete

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
    hydra_scene_path="${HYDRA_SCENE_PATH:-${DEFAULT_HYDRA_SCENE_PATH}}"

    mkdir -p "${project_root}/output"

    cd build
    cmake .. -Wno-dev \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DENABLE_GL_VK_CONVERSION=ON \
        -DENABLE_HYDRA=ON \
        -DCMAKE_INSTALL_PREFIX=${usd_path}/plugin/usd
    
    cmake --build . --target ${plugin_name} --config Release -j20
    cmake --install . --component ${plugin_name}
    cd ..

    export RT_SPP="${RT_SPP:-4}"

    echo "[hydra] RT_SPP=${RT_SPP}"
    echo "[hydra] HYDRA_SCENE_PATH=${hydra_scene_path}"
}

function show()
{
    hydra_scene_path="${HYDRA_SCENE_PATH:-${DEFAULT_HYDRA_SCENE_PATH}}"
    /home/yalu/software/usdtweak/build/usdtweak "${hydra_scene_path}"
}

function baseline(){
    echo "baseline visual regression has been removed."
    return 1
}

function selfcheck(){
    echo "selfcheck visual regression has been removed."
    return 1
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
