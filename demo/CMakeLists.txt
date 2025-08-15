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
find_package(pxr REQUIRED)
find_package(X11 REQUIRED)
find_package(MaterialX REQUIRED)
target_link_libraries(${PROJNAME} ${PXR_LIBRARIES} ${MATERIALX_LIBRARIES})
target_link_libraries(${PROJNAME} libheadless)
#--------------------------------------------------------------------------------------------------
# Linkage
#
target_compile_definitions(${PROJNAME} PRIVATE
  $<$<BOOL:${ENABLE_GL_VK_CONVERSION}>:ENABLE_GL_VK_CONVERSION=1>
  $<$<NOT:$<BOOL:${ENABLE_GL_VK_CONVERSION}>>:ENABLE_GL_VK_CONVERSION=0>
)
#------------------------------------------------------------------------------------------------
_finalize_target( ${PROJNAME} )