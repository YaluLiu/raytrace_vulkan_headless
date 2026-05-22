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
target_compile_definitions(${PROJNAME} PUBLIC -DPROJECT_RELDIRECTORY="../../../raster")

set(RASTER_PUBLIC_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/include/raster/aov_texture.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/raster/height_scan_types.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/raster/lidar_types.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/raster/raster_renderer.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/raster/raster_renderer_types.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/raster/raster_session.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/include/raster/shaders/host_device.h
  ${CMAKE_CURRENT_SOURCE_DIR}/include/raster/tile_config.hpp
)

set(RASTER_PRIVATE_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/private/core/raster_frame_executor.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/core/raster_output_controller.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/core/raster_renderer_internal.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/output/height_scan/height_scan.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/output/height_scan/height_scan_generation_pipeline.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/output/height_scan/height_scan_pass.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/output/lidar/lidar_depth_pipeline.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/output/lidar/lidar_point_cloud_pass.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/output/lidar/lidar_point_generation_pipeline.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/output/lidar/lidar_point_overlay_pipeline.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/output/lidar/lidar_scan.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/output/preview_raster_pipeline.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/output/tile/multiview_tile_raster_pipeline.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/output/tile/multiview_tile_targets.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/output/tile/tile_aov_channel.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/output/tile/tile_atlas_pass.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/raster_image_barriers.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/runtime/headless_vk.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/scene/raster_gpu_scene.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/scene/raster_rt_scene.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/scene/raster_scene_descriptors.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/scene/raster_scene_types.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/private/scene/raster_view_uniforms.hpp
)

set(RASTER_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/core/raster_descriptor_sets.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/core/raster_frame_executor.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/core/raster_output_controller.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/core/raster_renderer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/debug/raster_debug_readback.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/output/height_scan/height_scan.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/output/height_scan/height_scan_generation_pipeline.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/output/height_scan/height_scan_pass.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/output/lidar/lidar_depth_pipeline.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/output/lidar/lidar_point_cloud_pass.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/output/lidar/lidar_point_generation_pipeline.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/output/lidar/lidar_point_overlay_pipeline.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/output/lidar/lidar_scan.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/output/preview/preview_raster_pipeline.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/output/tile/multiview_tile_raster_pipeline.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/output/tile/multiview_tile_targets.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/output/tile/tile_aov_channel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/output/tile/tile_atlas_pass.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/output/tile/tile_atlas_renderer.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/headless_vk.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/raster_session.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/raster_geometry_upload.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/raster_gpu_scene.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/raster_rt_scene.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/raster_runtime_updates.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/raster_scene_descriptors.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/raster_scene_upload.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/scene/raster_view_uniforms.cpp
)

set(RASTER_SHADER_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/height_scan/height_scan.comp
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/lidar/lidar_depth.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/lidar/lidar_depth.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/lidar/lidar_overlay.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/lidar/lidar_overlay.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/lidar/lidar_points.comp
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/preview/dome_background.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/preview/dome_background.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/preview/raster.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/preview/raster.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/tile/dome_background_tile_multiview.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/tile/dome_background_tile_multiview.vert
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/tile/tile_multiview.frag
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/tile/tile_multiview.vert
)

set(RASTER_SHADER_HEADERS
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/common/dome_light.glsl
  ${CMAKE_CURRENT_SOURCE_DIR}/shaders/common/host_device.h
)

file(GLOB EXTRA_COMMON ${TUTO_KHR_DIR}/common/*.*)
list(APPEND COMMON_SOURCE_FILES ${EXTRA_COMMON})
include_directories(${TUTO_KHR_DIR}/common)


compile_glsl(
  SOURCE_FILES ${RASTER_SHADER_SOURCES}
  HEADER_FILES ${RASTER_SHADER_HEADERS}
  DST "${CMAKE_CURRENT_SOURCE_DIR}/spv"
  VULKAN_TARGET "vulkan1.2"
  DEPENDENCY ON
  FLAGS -g -I${CMAKE_CURRENT_SOURCE_DIR}/shaders/common
)


target_sources(${PROJNAME}
  PUBLIC
    ${RASTER_PUBLIC_HEADERS}
    ${COMMON_SOURCE_FILES}
  PRIVATE
    ${RASTER_PRIVATE_HEADERS}
    ${RASTER_SOURCES}
    ${PACKAGE_SOURCE_FILES}
    ${RASTER_SHADER_SOURCES}
    ${RASTER_SHADER_HEADERS}
)

target_include_directories(${PROJNAME}
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/common
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/private
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES
  ${RASTER_PUBLIC_HEADERS}
  ${RASTER_PRIVATE_HEADERS}
  ${RASTER_SOURCES}
  ${RASTER_SHADER_SOURCES}
  ${RASTER_SHADER_HEADERS}
)
source_group("Common" FILES ${COMMON_SOURCE_FILES} ${PACKAGE_SOURCE_FILES})

set(RASTER_PLATFORM_LIBRARIES ${PLATFORM_LIBRARIES})
if(OPENGL_LIBRARY)
  list(REMOVE_ITEM RASTER_PLATFORM_LIBRARIES ${OPENGL_LIBRARY})
endif()
target_link_libraries(${PROJNAME} ${RASTER_PLATFORM_LIBRARIES} nvpro_core)

foreach(DEBUGLIB ${LIBRARIES_DEBUG})
  target_link_libraries(${PROJNAME} debug ${DEBUGLIB})
endforeach(DEBUGLIB)

foreach(RELEASELIB ${LIBRARIES_OPTIMIZED})
  target_link_libraries(${PROJNAME} optimized ${RELEASELIB})
endforeach(RELEASELIB)

_finalize_target( ${PROJNAME} )


install(FILES ${SPV_OUTPUT} CONFIGURATIONS Release DESTINATION "bin_${ARCH}/${PROJNAME}/spv")
install(FILES ${SPV_OUTPUT} CONFIGURATIONS Debug DESTINATION "bin_${ARCH}_debug/${PROJNAME}/spv")

install(
  FILES ${SPV_OUTPUT}
  DESTINATION "./hdRobot/spv"
  COMPONENT hdRobot
)
