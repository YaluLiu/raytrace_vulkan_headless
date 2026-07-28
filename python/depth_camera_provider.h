#pragma once

#include <engine/engine.hpp>

#include <pybind11/numpy.h>

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace headless_training {
class TrainingSceneRuntime;
}

class DepthCameraProvider {
public:
  DepthCameraProvider(const std::string &usdPath, uint32_t cameraCount,
                      int width = 64, int height = 64,
                      const std::string &cameraPath = {},
                      const std::string &pluginSearchRoot = {});
  ~DepthCameraProvider();

  DepthCameraProvider(const DepthCameraProvider &) = delete;
  DepthCameraProvider &operator=(const DepthCameraProvider &) = delete;
  DepthCameraProvider(DepthCameraProvider &&) = delete;
  DepthCameraProvider &operator=(DepthCameraProvider &&) = delete;

  void updateCameraPoses(
      pybind11::array_t<float,
                        pybind11::array::c_style | pybind11::array::forcecast>
          positions,
      pybind11::array_t<float,
                        pybind11::array::c_style | pybind11::array::forcecast>
          quaternionsWxyz);

  pybind11::array_t<float> renderDepth(bool linearize = true);

  pybind11::array_t<float> computeFromCameraPoses(
      pybind11::array_t<float,
                        pybind11::array::c_style | pybind11::array::forcecast>
          positions,
      pybind11::array_t<float,
                        pybind11::array::c_style | pybind11::array::forcecast>
          quaternionsWxyz,
      bool linearize = true);

  uint32_t cameraCount() const {
    return static_cast<uint32_t>(m_cameraTemplates.size());
  }
  uint32_t width() const { return m_width; }
  uint32_t height() const { return m_height; }
  std::vector<std::string> cameraNames() const;

private:
  TileDepthFrame renderDepthFrame(bool linearize);

  std::mutex m_mutex;
  Engine m_engine;
  std::unique_ptr<headless_training::TrainingSceneRuntime> m_runtime;
  std::vector<CameraSpec> m_cameraTemplates;
  std::vector<CameraSpec> m_cameras;
  uint32_t m_width{0};
  uint32_t m_height{0};
};
