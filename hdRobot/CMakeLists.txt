set(PROJNAME hdRobot)


add_library(
  ${PROJNAME} SHARED
  instancer.cpp
  instancer.h
  backendScene.cpp
  backendScene.h
  backendHandles.h
  slotVector.h
  aovBridgeSpec.cpp
  aovBridgeSpec.h
  rasterBridge.cpp
  rasterBridge.h
  rasterBridgeConversions.cpp
  rasterBridgeConversions.h
  glInteropCache.cpp
  glInteropCache.h
  hydraRasterAovCopy.cpp
  hydraRasterAovCopy.h
  hydraTextureAssetExport.cpp
  hydraTextureAssetExport.h
  renderBuffer.cpp
  renderBuffer.h
  renderDelegate.cpp
  renderDelegate.h
  rendererPlugin.cpp
  rendererPlugin.h
  sceneData.cpp
  sceneData.h
  renderParam.cpp
  renderParam.h
  renderPass.cpp
  renderPass.h
  material.cpp
  material.h
  materialXParser.cpp
  materialXParser.h
  light.cpp
  light.h
  camera.cpp
  camera.h
  heightScanSensor.cpp
  heightScanSensor.h
  lidarSensor.cpp
  lidarSensor.h
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
    INSTALL_RPATH "$ORIGIN"
    # The other libs in the plugin dir have no "lib" prefix, so let's match this
    PREFIX ""
    # Remove _d postfix because of plugInfo.json mismatch on macOS
    DEBUG_POSTFIX ""
)

target_include_directories(
  ${PROJNAME}
  PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
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
  set(USD_LIBS_PRIVATE usdImaging usdMtlx hdMtlx glf hgi hgiGL hgiInterop hdx)
endif()

target_link_libraries(
  ${PROJNAME}
  PUBLIC
    ${USD_LIBS_PUBLIC} libraster
  PRIVATE
    ${USD_LIBS_PRIVATE}
    UsdRaySensor
    UsdRaySensorImaging
)

# Allow custom prefix for houdini directory structure
if(NOT DEFINED PLUGINFO_LIBPATH_PREFIX)
  set(PLUGINFO_LIBPATH_PREFIX "..")
endif()

set(PLUGINFO_PATH "${CMAKE_CURRENT_BINARY_DIR}/plugInfo.json")
set(PLUG_INFO_LIBRARY_PATH "${PLUGINFO_LIBPATH_PREFIX}/${PROJNAME}${CMAKE_SHARED_LIBRARY_SUFFIX}")
configure_file(plugInfo.json "${PLUGINFO_PATH}" @ONLY)



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
