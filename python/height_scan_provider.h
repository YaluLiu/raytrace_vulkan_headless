#pragma once

#include <engine/engine.hpp>
#include <engine/height_scan_types.hpp>

#include <pybind11/numpy.h>

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace headless_training {
class TrainingSceneRuntime;
}

class HeightScanProvider {
public:
  HeightScanProvider(const std::string &usdPath, int width = 64,
                     int height = 64, const std::string &pluginSearchRoot = {});
  ~HeightScanProvider();

  HeightScanProvider(const HeightScanProvider &) = delete;
  HeightScanProvider &operator=(const HeightScanProvider &) = delete;
  HeightScanProvider(HeightScanProvider &&) = delete;
  HeightScanProvider &operator=(HeightScanProvider &&) = delete;

  void setHeightScanParams(float uStart, float uEnd, float uStep, float vStart,
                           float vEnd, float vStep,
                           pybind11::tuple gravityDirectionWs, float maxRange);

  pybind11::array_t<float> computeFromSensorPoses(
      pybind11::array_t<float,
                        pybind11::array::c_style | pybind11::array::forcecast>
          positions,
      pybind11::array_t<float,
                        pybind11::array::c_style | pybind11::array::forcecast>
          quaternionsWxyz);

private:
  std::mutex m_mutex;
  Engine m_engine;
  std::unique_ptr<headless_training::TrainingSceneRuntime> m_runtime;
  HeightScanParams m_params;
  std::vector<std::string> m_sensorNames;
};
