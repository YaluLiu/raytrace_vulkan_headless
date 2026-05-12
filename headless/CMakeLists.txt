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
target_compile_definitions(${PROJNAME} PUBLIC -DPROJECT_RELDIRECTORY="../../../headless")

file(GLOB SOURCE_FILES *.cpp *.hpp *.inl *.h *.c)
list(APPEND SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/hello_vulkan_lidar.cpp")
list(APPEND SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/hello_vulkan_height_scan.cpp")
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

set(HEADLESS_PLATFORM_LIBRARIES ${PLATFORM_LIBRARIES})
if(OPENGL_LIBRARY)
  list(REMOVE_ITEM HEADLESS_PLATFORM_LIBRARIES ${OPENGL_LIBRARY})
endif()
target_link_libraries(${PROJNAME} ${HEADLESS_PLATFORM_LIBRARIES} nvpro_core)

foreach(DEBUGLIB ${LIBRARIES_DEBUG})
  target_link_libraries(${PROJNAME} debug ${DEBUGLIB})
endforeach(DEBUGLIB)

foreach(RELEASELIB ${LIBRARIES_OPTIMIZED})
  target_link_libraries(${PROJNAME} optimized ${RELEASELIB})
endforeach(RELEASELIB)

target_compile_definitions(${PROJNAME} PUBLIC
  $<$<BOOL:${ENABLE_GL_VK_CONVERSION}>:ENABLE_GL_VK_CONVERSION=1>
  $<$<NOT:$<BOOL:${ENABLE_GL_VK_CONVERSION}>>:ENABLE_GL_VK_CONVERSION=0>
)
target_compile_definitions(${PROJNAME} PUBLIC
  $<$<BOOL:${ENABLE_HYDRA}>:ENABLE_HYDRA=1>
  $<$<NOT:$<BOOL:${ENABLE_HYDRA}>>:ENABLE_HYDRA=0>
)

_finalize_target( ${PROJNAME} )


install(FILES ${SPV_OUTPUT} CONFIGURATIONS Release DESTINATION "bin_${ARCH}/${PROJNAME}/spv")
install(FILES ${SPV_OUTPUT} CONFIGURATIONS Debug DESTINATION "bin_${ARCH}_debug/${PROJNAME}/spv")

if(ENABLE_HYDRA)
  install(
    FILES ${SPV_OUTPUT}
    DESTINATION "./hdRobot/spv"
    COMPONENT hdRobot
  )
endif()
