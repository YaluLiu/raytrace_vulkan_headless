set(PROJNAME headless)
set(PROJNAME vk_${PROJNAME}_KHR)

#--------------------------------------------------------------------------------------------------
# C++ target and defines
add_executable(${PROJNAME} main.cpp)

# can not add here,because spv file path changed
# _add_project_definitions(${PROJNAME})

#--------------------------------------------------------------------------------------------------
# include
#
target_include_directories(${PROJNAME} PRIVATE
  ${PROJECT_SOURCE_DIR}/headless
  ${PROJECT_SOURCE_DIR}/common
)

#--------------------------------------------------------------------------------------------------
# Linkage
#
target_link_libraries(${PROJNAME} PRIVATE libheadless)

#------------------------------------------------------------------------------------------------
_finalize_target( ${PROJNAME} )


#------------------------------------------------------------------------------------------------
# can replace _add_project_definitions &&_finalize_target
# 
target_compile_definitions(${PROJNAME} PUBLIC -DPROJECT_NAME="${PROJNAME}")
target_compile_definitions(${PROJNAME} PUBLIC -DPROJECT_RELDIRECTORY="../../../headless")