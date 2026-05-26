set(PROJNAME hdRobotLidarUsd)

add_library(
  ${PROJNAME} SHARED
  lidarSensorAdapter.cpp
  lidarSensorAdapter.h
)

set_target_properties(
  ${PROJNAME}
  PROPERTIES
    INSTALL_RPATH_USE_LINK_PATH ON
    PREFIX ""
    DEBUG_POSTFIX ""
)

if(MSVC_VERSION GREATER_EQUAL 1920)
  target_compile_options(${PROJNAME} PRIVATE "/Zc:inline-")
endif()

find_package(pxr REQUIRED)

if(TARGET usd_ms)
  set(USD_LIBS_PUBLIC usd_ms)
else()
  set(USD_LIBS_PUBLIC hd)
  set(USD_LIBS_PRIVATE usd usdGeom usdImaging)
endif()

target_link_libraries(
  ${PROJNAME}
  PUBLIC
    ${USD_LIBS_PUBLIC}
  PRIVATE
    ${USD_LIBS_PRIVATE}
)

if(NOT DEFINED PLUGINFO_LIBPATH_PREFIX)
  set(PLUGINFO_LIBPATH_PREFIX "..")
endif()

set(PLUGINFO_PATH "${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json")
set(PLUG_INFO_LIBRARY_PATH "${PLUGINFO_LIBPATH_PREFIX}/${PROJNAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")
configure_file(plugInfo.json "${PLUGINFO_PATH}" @ONLY)

install(
  FILES
    "${PLUGINFO_PATH}"
    "${CMAKE_CURRENT_SOURCE_DIR}/generatedSchema.usda"
  DESTINATION
    "./${PROJNAME}/resources"
  COMPONENT
    ${PROJNAME}
)

install(
  TARGETS
    ${PROJNAME}
  LIBRARY
    DESTINATION .
    COMPONENT ${PROJNAME}
  RUNTIME
    DESTINATION .
    COMPONENT ${PROJNAME}
  ARCHIVE
    DESTINATION .
    COMPONENT ${PROJNAME}
)
