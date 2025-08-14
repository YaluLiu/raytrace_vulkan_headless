#!/bin/bash

BUILD_TYPE="Release"

function build() {
    set -e
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=${BUILD_TYPE}
    make -j20
    cd ..
}

app_name="ray_tracing_animation"
app_name="headless"

function format(){
    find headless -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i
}

function test() {
    build
    build/bin/${BUILD_TYPE}/vk_${app_name}_KHR_app
}

function headless() {
    app_name="demo_headless"
    test
}

function anim() {
    app_name="ray_tracing_animation"
    test
}


# 动态函数调用
if declare -f "$1" > /dev/null; then
    "$1" "${@:2}"  # 调用传入的函数，并传递额外的参数
else
    test
fi
