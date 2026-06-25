set(HEADLESS_TRAINING_CORE headlessTrainingCore)

find_package(pxr REQUIRED)

if(TARGET usd_ms)
  set(HEADLESS_TRAINING_USD_LIBS usd_ms)
else()
  set(HEADLESS_TRAINING_USD_LIBS usd usdGeom usdLux usdShade)
endif()

add_library(
  ${HEADLESS_TRAINING_CORE}
  STATIC
    cli.cpp
    cli.h
    output_writers.cpp
    output_writers.h
    preview_camera.cpp
    preview_camera.h
    training_runner.cpp
    training_runner.h
    training_scene.cpp
    training_scene.h
    usd_animation_source.cpp
    usd_animation_source.h
    usd_light_reader.cpp
    usd_light_reader.h
    usd_material_reader.cpp
    usd_material_reader.h
    usd_mesh_reader.cpp
    usd_mesh_reader.h
    usd_scene_loader.cpp
    usd_scene_loader.h
    usd_scene_reader_utils.cpp
    usd_scene_reader_utils.h
    usd_sensor_reader.cpp
    usd_sensor_reader.h
    usd_texture_registry.cpp
    usd_texture_registry.h
)

target_compile_features(${HEADLESS_TRAINING_CORE} PUBLIC cxx_std_20)

target_include_directories(
  ${HEADLESS_TRAINING_CORE}
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
  PRIVATE
    ${PROJECT_SOURCE_DIR}
)

target_link_libraries(
  ${HEADLESS_TRAINING_CORE}
  PUBLIC
    libengine
    UsdRaySensor
    ${HEADLESS_TRAINING_USD_LIBS}
)

add_executable(robot_training_headless main.cpp)
target_compile_features(robot_training_headless PRIVATE cxx_std_20)
target_link_libraries(robot_training_headless PRIVATE ${HEADLESS_TRAINING_CORE})

add_executable(robot_training_viewer
  viewer_main.cpp
  viewer_app.cpp
  viewer_app.h
  viewer_camera_controller.cpp
  viewer_camera_controller.h
  viewer_cli.cpp
  viewer_cli.h
  viewer_export.cpp
  viewer_export.h
  viewer_output_config.cpp
  viewer_output_config.h
)
target_compile_features(robot_training_viewer PRIVATE cxx_std_20)
target_link_libraries(robot_training_viewer PRIVATE ${HEADLESS_TRAINING_CORE} nvpro_core)

if(BUILD_TESTING)
  add_executable(headless_training_usd_scene_loader_test
    tests/usd_scene_loader_test.cpp
  )
  target_compile_features(headless_training_usd_scene_loader_test PRIVATE cxx_std_20)
  target_link_libraries(headless_training_usd_scene_loader_test PRIVATE ${HEADLESS_TRAINING_CORE})
  add_test(NAME headless_training_usd_scene_loader_test COMMAND headless_training_usd_scene_loader_test)

  add_executable(headless_training_usd_animation_output_test
    tests/usd_animation_output_test.cpp
  )
  target_compile_features(headless_training_usd_animation_output_test PRIVATE cxx_std_20)
  target_link_libraries(headless_training_usd_animation_output_test PRIVATE ${HEADLESS_TRAINING_CORE})
  add_test(NAME headless_training_usd_animation_output_test COMMAND headless_training_usd_animation_output_test)

  add_executable(headless_training_cli_test
    tests/cli_test.cpp
  )
  target_compile_features(headless_training_cli_test PRIVATE cxx_std_20)
  target_link_libraries(headless_training_cli_test PRIVATE ${HEADLESS_TRAINING_CORE})
  add_test(NAME headless_training_cli_test COMMAND headless_training_cli_test)

  add_executable(headless_training_preview_camera_test
    tests/preview_camera_test.cpp
  )
  target_compile_features(headless_training_preview_camera_test PRIVATE cxx_std_20)
  target_link_libraries(headless_training_preview_camera_test PRIVATE ${HEADLESS_TRAINING_CORE})
  add_test(NAME headless_training_preview_camera_test COMMAND headless_training_preview_camera_test)

  add_executable(headless_training_viewer_camera_controller_test
    tests/viewer_camera_controller_test.cpp
    viewer_camera_controller.cpp
    viewer_camera_controller.h
  )
  target_compile_features(headless_training_viewer_camera_controller_test PRIVATE cxx_std_20)
  target_link_libraries(headless_training_viewer_camera_controller_test PRIVATE ${HEADLESS_TRAINING_CORE})
  add_test(NAME headless_training_viewer_camera_controller_test COMMAND headless_training_viewer_camera_controller_test)

  add_test(NAME headless_training_help_test COMMAND robot_training_headless --help)
  set_tests_properties(headless_training_help_test PROPERTIES PASS_REGULAR_EXPRESSION "Usage: robot_training_headless")
endif()
