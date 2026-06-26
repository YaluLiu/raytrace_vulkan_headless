set(HEADLESS_TRAINING_CORE headlessTrainingCore)
set(HEADLESS_TRAINING_TRAIN headlessTrainingTrain)

find_package(pxr REQUIRED)

if(TARGET usd_ms)
  set(HEADLESS_TRAINING_USD_LIBS usd_ms)
else()
  set(HEADLESS_TRAINING_USD_LIBS usd usdGeom usdLux usdShade)
endif()

add_library(
  ${HEADLESS_TRAINING_CORE}
  STATIC
    core/output_writers.cpp
    core/output_writers.h
    core/preview_camera.cpp
    core/preview_camera.h
    core/training_scene.cpp
    core/training_scene.h
    core/usd_animation_source.cpp
    core/usd_animation_source.h
    core/usd_light_reader.cpp
    core/usd_light_reader.h
    core/usd_material_reader.cpp
    core/usd_material_reader.h
    core/usd_mesh_reader.cpp
    core/usd_mesh_reader.h
    core/usd_scene_loader.cpp
    core/usd_scene_loader.h
    core/usd_scene_reader_utils.cpp
    core/usd_scene_reader_utils.h
    core/usd_sensor_reader.cpp
    core/usd_sensor_reader.h
    core/usd_texture_registry.cpp
    core/usd_texture_registry.h
)

set_target_properties(${HEADLESS_TRAINING_CORE} PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_compile_features(${HEADLESS_TRAINING_CORE} PUBLIC cxx_std_20)

target_include_directories(
  ${HEADLESS_TRAINING_CORE}
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/core
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

add_library(
  ${HEADLESS_TRAINING_TRAIN}
  STATIC
    train/cli.cpp
    train/cli.h
    train/training_runner.cpp
    train/training_runner.h
)

target_compile_features(${HEADLESS_TRAINING_TRAIN} PUBLIC cxx_std_20)

target_include_directories(
  ${HEADLESS_TRAINING_TRAIN}
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/train
)

target_link_libraries(
  ${HEADLESS_TRAINING_TRAIN}
  PUBLIC
    ${HEADLESS_TRAINING_CORE}
)

add_executable(robot_training_headless train/main.cpp)
target_compile_features(robot_training_headless PRIVATE cxx_std_20)
target_link_libraries(robot_training_headless PRIVATE ${HEADLESS_TRAINING_TRAIN})

add_executable(robot_training_viewer
  viewer/viewer_main.cpp
  viewer/viewer_app.cpp
  viewer/viewer_app.h
  viewer/viewer_camera_controller.cpp
  viewer/viewer_camera_controller.h
  viewer/viewer_cli.cpp
  viewer/viewer_cli.h
  viewer/viewer_export.cpp
  viewer/viewer_export.h
  viewer/viewer_output_config.cpp
  viewer/viewer_output_config.h
)
target_compile_features(robot_training_viewer PRIVATE cxx_std_20)
target_include_directories(robot_training_viewer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/viewer)
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
  target_link_libraries(headless_training_cli_test PRIVATE ${HEADLESS_TRAINING_TRAIN})
  add_test(NAME headless_training_cli_test COMMAND headless_training_cli_test)

  add_executable(headless_training_preview_camera_test
    tests/preview_camera_test.cpp
  )
  target_compile_features(headless_training_preview_camera_test PRIVATE cxx_std_20)
  target_link_libraries(headless_training_preview_camera_test PRIVATE ${HEADLESS_TRAINING_CORE})
  add_test(NAME headless_training_preview_camera_test COMMAND headless_training_preview_camera_test)

  add_executable(headless_training_viewer_camera_controller_test
    tests/viewer_camera_controller_test.cpp
    viewer/viewer_camera_controller.cpp
    viewer/viewer_camera_controller.h
  )
  target_compile_features(headless_training_viewer_camera_controller_test PRIVATE cxx_std_20)
  target_include_directories(
    headless_training_viewer_camera_controller_test
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/viewer
  )
  target_link_libraries(headless_training_viewer_camera_controller_test PRIVATE ${HEADLESS_TRAINING_CORE})
  add_test(NAME headless_training_viewer_camera_controller_test COMMAND headless_training_viewer_camera_controller_test)

  add_test(NAME headless_training_help_test COMMAND robot_training_headless --help)
  set_tests_properties(headless_training_help_test PROPERTIES PASS_REGULAR_EXPRESSION "Usage: robot_training_headless")
endif()
