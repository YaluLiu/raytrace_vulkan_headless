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

file(GLOB SOURCE_FILES *.cpp *.hpp *.inl *.h *.c)
list(REMOVE_DUPLICATES SOURCE_FILES)
list(APPEND SOURCE_FILES
  ${CMAKE_CURRENT_SOURCE_DIR}/raster_frame_executor.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/raster_frame_executor.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/raster_gpu_scene.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/raster_gpu_scene.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/raster_output_controller.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/raster_output_controller.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/raster_renderer_types.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/raster_scene_descriptors.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/raster_scene_descriptors.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/raster_view_uniforms.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/raster_view_uniforms.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tile_atlas_pass.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tile_atlas_pass.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tile_aov_channel.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/tile_aov_channel.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/multiview_tile_targets.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/multiview_tile_targets.hpp
  ${CMAKE_CURRENT_SOURCE_DIR}/multiview_tile_raster_pipeline.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/multiview_tile_raster_pipeline.hpp
)
list(REMOVE_DUPLICATES SOURCE_FILES)
file(GLOB EXTRA_COMMON ${TUTO_KHR_DIR}/common/*.*)
list(APPEND COMMON_SOURCE_FILES ${EXTRA_COMMON})
include_directories(${TUTO_KHR_DIR}/common)


compile_glsl_directory(
	SRC "${CMAKE_CURRENT_SOURCE_DIR}/shaders" 
	DST "${CMAKE_CURRENT_SOURCE_DIR}/spv" 
	VULKAN_TARGET "vulkan1.2"
	DEPENDENCY ON
	)


target_sources(${PROJNAME} PUBLIC ${SOURCE_FILES} ${HEADER_FILES})
target_sources(${PROJNAME} PUBLIC ${COMMON_SOURCE_FILES})
target_sources(${PROJNAME} PUBLIC ${PACKAGE_SOURCE_FILES})
target_sources(${PROJNAME} PUBLIC ${GLSL_SOURCES} ${GLSL_HEADERS})

target_include_directories(${PROJNAME} PUBLIC
  ${PROJECT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/common
)

source_group("Common"       FILES ${COMMON_SOURCE_FILES} ${PACKAGE_SOURCE_FILES})
source_group("Sources"      FILES ${SOURCE_FILES})
source_group("Headers"      FILES ${HEADER_FILES})
source_group("Shader Sources"  FILES ${GLSL_SOURCES})
source_group("Shader Headers"  FILES ${GLSL_HEADERS})

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
