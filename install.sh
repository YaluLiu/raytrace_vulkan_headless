#!/bin/bash

BUILD_TYPE="Release"
app_name="headless"

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

function gatling(){
    set -e
    usd_path="/home/${USER}/software/USD"
    cd build
    cmake .. -Wno-dev \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DENABLE_GL_VK_CONVERSION=ON \
        -DENABLE_HYDRA=ON \
        -DCMAKE_INSTALL_PREFIX=${usd_path}/plugin/usd
    
    cmake --build . --target hdGatling --config Release -j20
    cmake --install . --component hdGatling
    cd ..
    /home/yalu/software/usdtweak/build/usdtweak
}


# 动态函数调用
if declare -f "$1" > /dev/null; then
    "$1" "${@:2}"  # 调用传入的函数，并传递额外的参数
else
    test
fi
