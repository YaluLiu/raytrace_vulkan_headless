#!/bin/bash

BUILD_TYPE="Release"
app_name="headless"

function build() {
    set -e
    cd build
    cmake ..  \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DENABLE_GL_VK_CONVERSION=ON
    make -j20
    cd ..
}

function format(){
    find headless -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i
}

function test() {
    build
    build/bin/${BUILD_TYPE}/vk_${app_name}_KHR_app
    # build/bin/${BUILD_TYPE}/libheadless_app
}

function anim() {
    app_name="ray_tracing_animation"
    build
    build/bin/${BUILD_TYPE}/vk_${app_name}_KHR_app
}



# 动态函数调用
if declare -f "$1" > /dev/null; then
    "$1" "${@:2}"  # 调用传入的函数，并传递额外的参数
else
    test
fi
