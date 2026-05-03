cmake_minimum_required(VERSION 3.9.6 FATAL_ERROR)
project(vk_raytracing_tutorial)
include(CTest)

#--------------------------------------------------------------------------------------------------
# look for nvpro_core 1) as a sub-folder 2) at some other locations
# this cannot be put anywhere else since we still didn't find setup.cmake yet
if(NOT BASE_DIRECTORY)

  find_path(BASE_DIRECTORY
    NAMES nvpro_core/cmake/setup.cmake
    PATHS ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/.. ${CMAKE_CURRENT_SOURCE_DIR}/../.. 
    REQUIRED
    DOC "Directory containing nvpro_core"
    )
endif()

# add_subdirectory(usd_rt)

## Various functions and macros REQUIRED
if(EXISTS ${BASE_DIRECTORY}/nvpro_core/cmake/setup.cmake)
  include(${BASE_DIRECTORY}/nvpro_core/cmake/setup.cmake)
  include(${BASE_DIRECTORY}/nvpro_core/cmake/utilities.cmake)
else()
  message(FATAL_ERROR "could not find base directory, please set BASE_DIRECTORY to folder containing nvpro_core")
endif()


set(TUTO_KHR_DIR ${CMAKE_CURRENT_SOURCE_DIR})

if(MSVC)
    add_definitions(/wd26812)  # 'enum class' over 'enum'
    add_definitions(/wd26451)  # Arithmetic overflow, casting 4 byte value to 8 byte value
endif()

#--------------------------------------------------------------------------------------------------
# Package shared by all projects
_add_package_OpenGL()
_add_package_VulkanSDK()
_add_package_ImGUI()
_add_nvpro_core_lib()

message(STATUS "COPY ${CMAKE_CURRENT_SOURCE_DIR}/media  to  ${OUTPUT_PATH}")
file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/media DESTINATION ${OUTPUT_PATH})


#--------------------------------------------------------------------------------------------------
# compile option
# convert vulkan to opengl for hydra plugin
option(ENABLE_HYDRA "Enable OpenUsd hydra plugins" OFF)
option(ENABLE_GL_VK_CONVERSION "Enable OpenGL/Vulkan conversion" ON)
option(ENABLE_DLSS_RR "Enable NVIDIA NGX DLSS-RR denoiser integration" ON)
set(DLSS_SDK_ROOT "" CACHE PATH "Root path of NVIDIA DLSS SDK (contains include/ and lib/)")
# 让子目录可以访问
set(ENABLE_GL_VK_CONVERSION ${ENABLE_GL_VK_CONVERSION} CACHE BOOL "Enable OpenGL/Vulkan conversion")
set(ENABLE_HYDRA ${ENABLE_HYDRA} CACHE BOOL "Enable OpenUsd hydra plugins")
set(ENABLE_DLSS_RR ${ENABLE_DLSS_RR} CACHE BOOL "Enable NVIDIA NGX DLSS-RR denoiser integration")
set(DLSS_SDK_ROOT ${DLSS_SDK_ROOT} CACHE PATH "Root path of NVIDIA DLSS SDK (contains include/ and lib/)")

message(STATUS "ENABLE_GL_VK_CONVERSION: ${ENABLE_GL_VK_CONVERSION}")
message(STATUS "ENABLE_DLSS_RR: ${ENABLE_DLSS_RR}")
#--------------------------------------------------------------------------------------------------
#--------------------------------------------------------------------------------------------------
# Contract tests
if(BUILD_TESTING)
  find_package(PythonInterp 3 REQUIRED)

  function(add_python_contract_test test_name script_path)
    add_test(
      NAME ${test_name}
      COMMAND "${PYTHON_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/${script_path}"
    )
    set_tests_properties(${test_name} PROPERTIES
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      LABELS "contracts;python"
    )
  endfunction()

  add_python_contract_test(contracts.dlss_rr_shader scripts/check_dlss_rr_shader_contracts.py)
  add_python_contract_test(contracts.dlss_rr_supported_size scripts/check_dlss_rr_supported_size.py)
  add_python_contract_test(contracts.lidar_after_dlss scripts/check_lidar_after_dlss_contracts.py)
  add_python_contract_test(contracts.renderer_architecture scripts/check_renderer_architecture_contracts.py)
  add_python_contract_test(contracts.material_texture_in_memory scripts/check_material_texture_in_memory_contracts.py)
  add_python_contract_test(contracts.hydra_entrypoint scripts/check_hydra_entrypoint_contracts.py)
  add_python_contract_test(contracts.pbr_material scripts/check_pbr_material_contracts.py)
endif()
#--------------------------------------------------------------------------------------------------
#--------------------------------------------------------------------------------------------------
# Sub examples
add_subdirectory(headless)
add_subdirectory(demo)
add_subdirectory(hdRobot)
#--------------------------------------------------------------------------------------------------
# Install - copying the media directory
install(DIRECTORY "media" 
        CONFIGURATIONS Release 
        DESTINATION "bin_${ARCH}")
install(DIRECTORY "media" 
        CONFIGURATIONS Debug 
        DESTINATION "bin_${ARCH}_debug")
