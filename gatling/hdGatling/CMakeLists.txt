set(PROJNAME hdGatling)


add_library(
  ${PROJNAME} SHARED
  instancer.cpp
  instancer.h
  renderBuffer.cpp
  renderBuffer.h
  renderDelegate.cpp
  renderDelegate.h
  rendererPlugin.cpp
  rendererPlugin.h
  renderParam.cpp
  renderParam.h
  renderPass.cpp
  renderPass.h
  renderScene.cpp
  renderScene.h
  material.cpp
  material.h
  mesh.cpp
  mesh.h
  tokens.cpp
  tokens.h
  utils.cpp
  utils.h
)

set_target_properties(
  ${PROJNAME}
  PROPERTIES
    INSTALL_RPATH_USE_LINK_PATH ON
    # The other libs in the plugin dir have no "lib" prefix, so let's match this
    PREFIX ""
    # Remove _d postfix because of plugInfo.json mismatch on macOS
    DEBUG_POSTFIX ""
)

target_include_directories(
  ${PROJNAME}
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${PROJECT_SOURCE_DIR}/common"
  PRIVATE
    src
)

target_compile_definitions(
  ${PROJNAME}
  PUBLIC
    # Required for PLUG_THIS_PLUGIN macro
    MFB_PACKAGE_NAME=${PROJNAME}
  PRIVATE
    GI_LICENSE_FILE_NAME="LICENSE"
    # Workaround for https://github.com/PixarAnimationStudios/USD/issues/1471#issuecomment-799813477
    # "$<$<OR:$<CONFIG:>,$<CONFIG:Debug>>:TBB_USE_DEBUG>"
)

# Workaround for https://github.com/PixarAnimationStudios/USD/issues/1279
if(MSVC_VERSION GREATER_EQUAL 1920)
  target_compile_options(${PROJNAME} PRIVATE "/Zc:inline-")
endif()

find_package(pxr REQUIRED)

if(TARGET usd_ms)
  set(USD_LIBS_PUBLIC usd_ms)
else()
  set(USD_LIBS_PUBLIC hd)
  set(USD_LIBS_PRIVATE usdImaging usdMtlx hdMtlx glf)
endif()

target_link_libraries(
  ${PROJNAME}
  PUBLIC
    ${USD_LIBS_PUBLIC} libheadless
  PRIVATE
    ${USD_LIBS_PRIVATE}
)

# Allow custom prefix for houdini directory structure
if(NOT DEFINED PLUGINFO_LIBPATH_PREFIX)
  set(PLUGINFO_LIBPATH_PREFIX "..")
endif()

set(PLUGINFO_PATH "${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json")
set(PLUG_INFO_LIBRARY_PATH "${PLUGINFO_LIBPATH_PREFIX}/${PROJNAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")
configure_file(plugInfo.json.in "${PLUGINFO_PATH}" @ONLY)



install(
  FILES
    "${PLUGINFO_PATH}"
    "${PROJECT_SOURCE_DIR}/LICENSE"
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
