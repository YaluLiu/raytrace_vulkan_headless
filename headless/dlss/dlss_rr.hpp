#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <glm/mat4x4.hpp>
#include <vulkan/vulkan_core.h>

namespace dlss {

enum class PerfQuality
{
  MaxPerformance,
  Balanced,
  MaxQuality,
  UltraPerformance,
  UltraQuality,
  DLAA,
};

struct ImageInput
{
  VkImage       image{VK_NULL_HANDLE};
  VkImageView   view{VK_NULL_HANDLE};
  VkFormat      format{VK_FORMAT_UNDEFINED};
  VkImageLayout layout{VK_IMAGE_LAYOUT_GENERAL};
};

struct InitInputs
{
  VkInstance       instance{VK_NULL_HANDLE};
  VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
  VkDevice         device{VK_NULL_HANDLE};

  std::string              applicationDataPath;
  std::vector<std::string> featureSearchPaths;
};

struct EvaluateInputs
{
  VkCommandBuffer cmd{VK_NULL_HANDLE};
  uint32_t        renderWidth{0};
  uint32_t        renderHeight{0};
  uint32_t        targetWidth{0};
  uint32_t        targetHeight{0};
  PerfQuality     perfQuality{PerfQuality::Balanced};

  bool  reset{false};
  float jitterX{0.0f};
  float jitterY{0.0f};
  float mvScaleX{1.0f};
  float mvScaleY{1.0f};
  float frameTimeMs{16.6667f};

  glm::mat4 worldToView{1.0f};
  glm::mat4 viewToClip{1.0f};

  ImageInput color;
  ImageInput output;
  ImageInput diffuseAlbedo;
  ImageInput specularAlbedo;
  ImageInput normalsRoughness;
  ImageInput motionVectors;
  ImageInput depth;
  ImageInput specularHitDistance;
};

struct OptimalSettings
{
  uint32_t renderWidth{0};
  uint32_t renderHeight{0};
  uint32_t minRenderWidth{0};
  uint32_t minRenderHeight{0};
  uint32_t maxRenderWidth{0};
  uint32_t maxRenderHeight{0};
  float    sharpness{0.0f};
};

class DlssRR
{
public:
  DlssRR();
  ~DlssRR();

  DlssRR(DlssRR&&) noexcept;
  DlssRR& operator=(DlssRR&&) noexcept;

  DlssRR(const DlssRR&)            = delete;
  DlssRR& operator=(const DlssRR&) = delete;

  bool initialize(const InitInputs& inputs);
  void shutdown();

  bool queryOptimalSettings(uint32_t targetWidth, uint32_t targetHeight, PerfQuality quality, OptimalSettings& settings);
  bool evaluate(const EvaluateInputs& inputs);

  bool isApiEnabled() const;
  bool isOperational() const;

  const std::string& getLastError() const;

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

}
