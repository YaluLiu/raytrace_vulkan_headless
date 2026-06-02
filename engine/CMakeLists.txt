#*****************************************************************************
# Copyright 2020 NVIDIA Corporation. All rights reserved.
#*****************************************************************************

cmake_minimum_required(VERSION 3.9.6 FATAL_ERROR)

get_filename_component(PROJNAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(PROJNAME lib${PROJNAME})
project(${PROJNAME} LANGUAGES C CXX)
message(STATUS "-------------------------------")
message(STATUS "Processing Project ${PROJNAME}:")


set(CMAKE_CXX_STANDARD 20)
add_library(${PROJNAME})

# replace _add_project_definitions,because add_definitions can not send to Caller
# _add_project_definitions(${PROJNAME})
target_compile_definitions(${PROJNAME} PUBLIC -DPROJECT_NAME="${PROJNAME}")
target_compile_definitions(${PROJNAME} PUBLIC -DPROJECT_RELDIRECTORY="../../../engine")

set(ENGINE_PUBLIC_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/include/engine/aov_texture.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/engine/engine.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/engine/height_scan_types.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/engine/lidar_types.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/engine/mesh_types.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/engine/output_config.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/engine/renderer_types.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/engine/shaders/host_device.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/engine/texture_asset.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/engine/tile_config.hpp
)

set(ENGINE_PRIVATE_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/core/frame_executor.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/core/output_controller.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/core/renderer_internal.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/core/renderer_resources.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/height_scan/height_scan.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/height_scan/height_scan_generation_pipeline.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/height_scan/height_scan_pass.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/lidar/lidar_point_cloud_pass.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/lidar/lidar_point_generation_pipeline.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/lidar/lidar_scan.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/point_overlay/point_overlay_pipeline.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/preview/preview_pipeline.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/tile/multiview_tile_pipeline.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/tile/multiview_tile_targets.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/tile/tile_aov_channel.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/tile/tile_atlas_pass.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/image_barriers.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/headless_vk.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/gpu_scene.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/rt_scene.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/scene_descriptors.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/scene_types.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/view_uniforms.hpp
)

set(ENGINE_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/core/frame_executor.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/core/output_controller.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/core/renderer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/core/renderer_resources.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/debug/debug_readback.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/height_scan/height_scan.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/height_scan/height_scan_generation_pipeline.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/height_scan/height_scan_pass.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/lidar/lidar_point_cloud_pass.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/lidar/lidar_point_generation_pipeline.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/lidar/lidar_scan.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/point_overlay/point_overlay_pipeline.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/preview/preview_pipeline.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/tile/multiview_tile_pipeline.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/tile/multiview_tile_targets.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/tile/tile_aov_channel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/tile/tile_atlas_pass.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/headless_vk.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/session.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/gpu_scene.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/rt_scene.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/renderer_scene.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/scene_descriptors.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/view_uniforms.cpp
)

set(ENGINE_SHADER_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/features/height_scan/shaders/height_scan.comp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/height_scan/shaders/height_scan_overlay.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/features/height_scan/shaders/height_scan_overlay.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/features/lidar/shaders/lidar_overlay.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/features/lidar/shaders/lidar_overlay.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/features/lidar/shaders/lidar_points.comp
  ${CMAKE_CURRENT_SOURCE_DIR}/features/preview/shaders/dome_background.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/features/preview/shaders/dome_background.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/features/preview/shaders/mesh.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/features/preview/shaders/mesh.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/features/tile/shaders/dome_background_tile_multiview.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/features/tile/shaders/dome_background_tile_multiview.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/features/tile/shaders/tile_multiview.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/features/tile/shaders/tile_multiview.vert
)

set(ENGINE_SHADER_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/common/dome_light.glsl
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/common/host_device.h
)

file(GLOB EXTRA_COMMON ${TUTO_KHR_DIR}/common/*.*)
list(APPEND COMMON_SOURCE_FILES ${EXTRA_COMMON})
include_directories(${TUTO_KHR_DIR}/common)


compile_glsl(
  SOURCE_FILES ${ENGINE_SHADER_SOURCES}
  HEADER_FILES ${ENGINE_SHADER_HEADERS}
  DST "${CMAKE_CURRENT_SOURCE_DIR}/spv"
  VULKAN_TARGET "vulkan1.2"
  DEPENDENCY ON
  FLAGS -g -I${CMAKE_CURRENT_SOURCE_DIR}/shaders/common
)


target_sources(${PROJNAME}
  PUBLIC
    ${ENGINE_PUBLIC_HEADERS}
    ${COMMON_SOURCE_FILES}
  PRIVATE
    ${ENGINE_PRIVATE_HEADERS}
    ${ENGINE_SOURCES}
    ${PACKAGE_SOURCE_FILES}
    ${ENGINE_SHADER_SOURCES}
    ${ENGINE_SHADER_HEADERS}
)

target_include_directories(${PROJNAME}
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
  ${ENGINE_PUBLIC_HEADERS}
  ${ENGINE_PRIVATE_HEADERS}
  ${ENGINE_SOURCES}
  ${ENGINE_SHADER_SOURCES}
  ${ENGINE_SHADER_HEADERS}
)
source_group("Common" FILES ${COMMON_SOURCE_FILES} ${PACKAGE_SOURCE_FILES})

set(ENGINE_PLATFORM_LIBRARIES ${PLATFORM_LIBRARIES})
if(OPENGL_LIBRARY)
  list(REMOVE_ITEM ENGINE_PLATFORM_LIBRARIES ${OPENGL_LIBRARY})
endif()
target_link_libraries(${PROJNAME} ${ENGINE_PLATFORM_LIBRARIES} nvpro_core)

foreach(DEBUGLIB ${LIBRARIES_DEBUG})
  target_link_libraries(${PROJNAME} debug ${DEBUGLIB})
endforeach(DEBUGLIB)

foreach(RELEASELIB ${LIBRARIES_OPTIMIZED})
  target_link_libraries(${PROJNAME} optimized ${RELEASELIB})
endforeach(RELEASELIB)

_finalize_target( ${PROJNAME} )

if(BUILD_TESTING)
  add_executable(engine_height_scan_basis_test
    ${CMAKE_CURRENT_SOURCE_DIR}/tests/height_scan_basis_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/features/height_scan/height_scan.cpp
  )
  target_include_directories(engine_height_scan_basis_test
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
      ${CMAKE_CURRENT_SOURCE_DIR}/include
      ${CMAKE_CURRENT_SOURCE_DIR}
  )
  add_test(NAME engine_height_scan_basis_test COMMAND engine_height_scan_basis_test)
endif()


install(FILES ${SPV_OUTPUT} CONFIGURATIONS Release DESTINATION "bin_${ARCH}/${PROJNAME}/spv")
install(FILES ${SPV_OUTPUT} CONFIGURATIONS Debug DESTINATION "bin_${ARCH}_debug/${PROJNAME}/spv")

install(
  FILES ${SPV_OUTPUT}
  DESTINATION "./hdRobot/spv"
  COMPONENT hdRobot
)
