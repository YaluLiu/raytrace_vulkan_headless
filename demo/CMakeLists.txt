set(PROJNAME headless)
set(PROJNAME vk_${PROJNAME}_KHR)

#--------------------------------------------------------------------------------------------------
# C++ target and defines
file(GLOB SOURCE_FILES *.cpp *.hpp *.inl *.h *.c)
add_executable(${PROJNAME} ${SOURCE_FILES})

# can not add here,because spv file path changed
# _add_project_definitions(${PROJNAME})

#--------------------------------------------------------------------------------------------------
# Linkage
#
# pxr needs MaterialX，MaterialX needs x11
find_package(pxr REQUIRED)
find_package(X11 REQUIRED)
find_package(MaterialX REQUIRED)
target_link_libraries(${PROJNAME} ${PXR_LIBRARIES} ${MATERIALX_LIBRARIES})
target_link_libraries(${PROJNAME} libheadless)
#------------------------------------------------------------------------------------------------
_finalize_target( ${PROJNAME} )