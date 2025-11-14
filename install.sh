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
    cd build
    cmake ..  \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DENABLE_GL_VK_CONVERSION=ON \
        -DENABLE_HYDRA=OFF
    make -j20
    cd ..
    build/bin/${BUILD_TYPE}/vk_${app_name}_KHR_app
    # build/bin/${BUILD_TYPE}/libheadless_app
}

function anim() {
    app_name="ray_tracing_animation"
    set -e
    cd build
    cmake ..  \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DENABLE_GL_VK_CONVERSION=ON \
        -DENABLE_HYDRA=OFF
    make -j20
    cd ..
    build/bin/${BUILD_TYPE}/vk_${app_name}_KHR_app
}

function hydra(){
    set -e
    usd_path="/home/${USER}/software/USD"
    plugin_name="hdRobot"
    cd build
    cmake .. -Wno-dev \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DENABLE_GL_VK_CONVERSION=ON \
        -DENABLE_HYDRA=ON \
        -DCMAKE_INSTALL_PREFIX=${usd_path}/plugin/usd
    
    cmake --build . --target ${plugin_name} --config Release -j20
    cmake --install . --component ${plugin_name}
    cd ..
    /home/yalu/software/usdtweak/build/usdtweak
}


# 动态函数调用
if declare -f "$1" > /dev/null; then
    "$1" "${@:2}"  # 调用传入的函数，并传递额外的参数
else
    hydra
fi
