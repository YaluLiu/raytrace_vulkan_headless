find_package(Python3 COMPONENTS Interpreter Development.Module NumPy REQUIRED)

find_package(pybind11 CONFIG QUIET)
if(NOT pybind11_FOUND)
  execute_process(
    COMMAND ${Python3_EXECUTABLE} -m pybind11 --cmakedir
    OUTPUT_VARIABLE ROBOT_RASTER_PYBIND11_CMAKE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE ROBOT_RASTER_PYBIND11_CMAKE_DIR_RESULT
  )
  if(ROBOT_RASTER_PYBIND11_CMAKE_DIR_RESULT EQUAL 0 AND EXISTS "${ROBOT_RASTER_PYBIND11_CMAKE_DIR}")
    set(pybind11_DIR "${ROBOT_RASTER_PYBIND11_CMAKE_DIR}")
    find_package(pybind11 CONFIG QUIET)
  endif()
endif()

if(NOT pybind11_FOUND)
  message(FATAL_ERROR
    "pybind11 is required for ROBOT_ENGINE_BUILD_PYTHON=ON. "
    "Install it with `python -m pip install pybind11` or set pybind11_DIR.")
endif()

pybind11_add_module(robot_raster_py
  robot_raster_py.cpp
  height_scan_provider.cpp
  height_scan_provider.h
)

target_compile_features(robot_raster_py PRIVATE cxx_std_20)
target_include_directories(robot_raster_py
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/headlessTraining/core
)
target_link_libraries(robot_raster_py PRIVATE headlessTrainingCore)
