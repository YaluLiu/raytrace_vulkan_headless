#*****************************************************************************
# Copyright 2020 NVIDIA Corporation. All rights reserved.
#*****************************************************************************

cmake_minimum_required(VERSION 3.9.6 FATAL_ERROR)

#--------------------------------------------------------------------------------------------------
# Project setting
get_filename_component(PROJNAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(PROJNAME lib${PROJNAME})
project(${PROJNAME} LANGUAGES C CXX)
message(STATUS "-------------------------------")
message(STATUS "Processing Project ${PROJNAME}:")


#--------------------------------------------------------------------------------------------------
# C++ target and defines
set(CMAKE_CXX_STANDARD 20)
add_library(${PROJNAME})

#------------------------------------------------------------------------------------------------
# replace _add_project_definitions,because add_definitions can not send to Caller
# _add_project_definitions(${PROJNAME})
target_compile_definitions(${PROJNAME} PUBLIC -DPROJECT_NAME="${PROJNAME}")
target_compile_definitions(${PROJNAME} PUBLIC -DPROJECT_RELDIRECTORY="../../../headless")

#--------------------------------------------------------------------------------------------------
# Source files for this project
#
file(GLOB SOURCE_FILES *.cpp *.hpp *.inl *.h *.c)
list(APPEND SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/hello_vulkan_lidar.cpp")
list(APPEND SOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/hello_vulkan_height_scan.cpp")
list(REMOVE_DUPLICATES SOURCE_FILES)
file(GLOB DLSS_SOURCE_FILES dlss/*.cpp dlss/*.hpp)
file(GLOB EXTRA_COMMON ${TUTO_KHR_DIR}/common/*.*)
list(APPEND COMMON_SOURCE_FILES ${EXTRA_COMMON})
include_directories(${TUTO_KHR_DIR}/common)


#--------------------------------------------------------------------------------------------------
# GLSL to SPIR-V custom build
compile_glsl_directory(
	SRC "${CMAKE_CURRENT_SOURCE_DIR}/shaders" 
	DST "${CMAKE_CURRENT_SOURCE_DIR}/spv" 
	VULKAN_TARGET "vulkan1.2"
	DEPENDENCY ON
	)


#--------------------------------------------------------------------------------------------------
# Sources
target_sources(${PROJNAME} PUBLIC ${SOURCE_FILES} ${HEADER_FILES} ${DLSS_SOURCE_FILES})
target_sources(${PROJNAME} PUBLIC ${COMMON_SOURCE_FILES})
target_sources(${PROJNAME} PUBLIC ${PACKAGE_SOURCE_FILES})
target_sources(${PROJNAME} PUBLIC ${GLSL_SOURCES} ${GLSL_HEADERS})

#--------------------------------------------------------------------------------------------------
# include
#
target_include_directories(${PROJNAME} PUBLIC
  ${PROJECT_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/common
)

#--------------------------------------------------------------------------------------------------
# Sub-folders in Visual Studio
#
source_group("Common"       FILES ${COMMON_SOURCE_FILES} ${PACKAGE_SOURCE_FILES})
source_group("Sources"      FILES ${SOURCE_FILES})
source_group("DLSS"         FILES ${DLSS_SOURCE_FILES})
source_group("Headers"      FILES ${HEADER_FILES})
source_group("Shader Sources"  FILES ${GLSL_SOURCES})
source_group("Shader Headers"  FILES ${GLSL_HEADERS})

#--------------------------------------------------------------------------------------------------
# Linkage 
# 
target_link_libraries(${PROJNAME} ${PLATFORM_LIBRARIES} nvpro_core)

foreach(DEBUGLIB ${LIBRARIES_DEBUG})
  target_link_libraries(${PROJNAME} debug ${DEBUGLIB})
endforeach(DEBUGLIB)

foreach(RELEASELIB ${LIBRARIES_OPTIMIZED})
  target_link_libraries(${PROJNAME} optimized ${RELEASELIB})
endforeach(RELEASELIB)

if(WIN32)
  target_link_libraries(${PROJNAME} opengl32)
elseif(UNIX AND NOT APPLE)
  target_link_libraries(${PROJNAME} EGL GL)
endif()

set(DLSS_RR_FOUND OFF)
if(ENABLE_DLSS_RR)
  set(_DLSS_HINT_ROOTS)
  if(DLSS_SDK_ROOT)
    list(APPEND _DLSS_HINT_ROOTS ${DLSS_SDK_ROOT})
  endif()
  if(DEFINED ENV{DLSS_SDK_ROOT} AND NOT "$ENV{DLSS_SDK_ROOT}" STREQUAL "")
    list(APPEND _DLSS_HINT_ROOTS $ENV{DLSS_SDK_ROOT})
  endif()
  if(DEFINED ENV{NVSDK_NGX_SDK_ROOT} AND NOT "$ENV{NVSDK_NGX_SDK_ROOT}" STREQUAL "")
    list(APPEND _DLSS_HINT_ROOTS $ENV{NVSDK_NGX_SDK_ROOT})
  endif()

  find_path(DLSS_SDK_INCLUDE_DIR
            NAMES nvsdk_ngx_vk.h
            HINTS ${_DLSS_HINT_ROOTS}
            PATH_SUFFIXES include)

  if(WIN32)
    set(_DLSS_SDK_LIBRARY_NAMES nvsdk_ngx_d nvsdk_ngx nvsdk_ngx_khr_d nvsdk_ngx_s nvsdk_ngx_khr_s)
    set(_DLSS_SDK_LIBRARY_SUFFIXES lib/Windows_x86_64/x64 lib/Windows_x86_64/khr/x64)
    set(_DLSS_RR_DEFAULT_NGX_SUFFIX lib/Windows_x86_64/rel)
    if(DLSS_SDK_LIBRARY MATCHES "nvsdk_ngx(_khr)?_s\\.lib$")
      unset(DLSS_SDK_LIBRARY CACHE)
    endif()
  else()
    set(_DLSS_SDK_LIBRARY_NAMES nvsdk_ngx)
    set(_DLSS_SDK_LIBRARY_SUFFIXES lib/Linux_x86_64)
    set(_DLSS_RR_DEFAULT_NGX_SUFFIX lib/Linux_x86_64/rel)
  endif()

  find_library(DLSS_SDK_LIBRARY
               NAMES ${_DLSS_SDK_LIBRARY_NAMES}
               HINTS ${_DLSS_HINT_ROOTS}
               PATH_SUFFIXES ${_DLSS_SDK_LIBRARY_SUFFIXES})

  if(DLSS_SDK_INCLUDE_DIR AND DLSS_SDK_LIBRARY)
    set(DLSS_RR_FOUND ON)
    message(STATUS "DLSS SDK include: ${DLSS_SDK_INCLUDE_DIR}")
    message(STATUS "DLSS SDK library: ${DLSS_SDK_LIBRARY}")
    target_include_directories(${PROJNAME} PUBLIC ${DLSS_SDK_INCLUDE_DIR})
    target_link_libraries(${PROJNAME} ${DLSS_SDK_LIBRARY})
    if(UNIX AND NOT APPLE)
      target_link_libraries(${PROJNAME} dl)
    endif()

    set(_DLSS_RR_DEFAULT_NGX_PATH "")
    foreach(_DLSS_HINT_ROOT ${_DLSS_HINT_ROOTS})
      if(EXISTS "${_DLSS_HINT_ROOT}/${_DLSS_RR_DEFAULT_NGX_SUFFIX}")
        file(TO_CMAKE_PATH "${_DLSS_HINT_ROOT}/${_DLSS_RR_DEFAULT_NGX_SUFFIX}" _DLSS_RR_DEFAULT_NGX_PATH)
        break()
      endif()
    endforeach()
    if(EXISTS ${_DLSS_RR_DEFAULT_NGX_PATH})
      target_compile_definitions(${PROJNAME} PUBLIC DLSS_RR_DEFAULT_NGX_PATH="${_DLSS_RR_DEFAULT_NGX_PATH}")
    endif()
  else()
    message(WARNING "ENABLE_DLSS_RR is ON but NVIDIA DLSS SDK was not found. Building with DLSS-RR disabled.")
  endif()
endif()
#--------------------------------------------------------------------------------------------------
# option
#
target_compile_definitions(${PROJNAME} PUBLIC
  $<$<BOOL:${ENABLE_GL_VK_CONVERSION}>:ENABLE_GL_VK_CONVERSION=1>
  $<$<NOT:$<BOOL:${ENABLE_GL_VK_CONVERSION}>>:ENABLE_GL_VK_CONVERSION=0>
)
target_compile_definitions(${PROJNAME} PUBLIC
  $<$<BOOL:${ENABLE_HYDRA}>:ENABLE_HYDRA=1>
  $<$<NOT:$<BOOL:${ENABLE_HYDRA}>>:ENABLE_HYDRA=0>
)
target_compile_definitions(${PROJNAME} PUBLIC
  $<$<BOOL:${DLSS_RR_FOUND}>:ENABLE_DLSS_RR=1>
  $<$<NOT:$<BOOL:${DLSS_RR_FOUND}>>:ENABLE_DLSS_RR=0>
)
#--------------------------------------------------------------------------------------------------
# copies binaries that need to be put next to the exe files (ZLib, etc.)
#
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
