#pragma once

#include "training_scene.h"
#include "usd_animation_source.h"
#include "viewer_camera_controller.h"
#include "viewer_output_config.h"

#include <engine/engine.hpp>

#include <nvvk/context_vk.hpp>
#include <nvvkhl/appbase_vk.hpp>
#include <imgui.h>

#include <filesystem>
#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <vector>

struct GLFWwindow;

namespace headless_training
{

struct ViewerCliOptions
{
  std::filesystem::path usdPath;
  std::string cameraPath;
  std::string pluginSearchRoot;
  std::filesystem::path outputDir;
  int width{1280};
  int height{720};
  bool enableLidar{true};
  bool enableHeightScan{true};
  bool showHelp{false};
};

struct ViewerCliParseResult
{
  ViewerCliOptions options;
  bool ok{false};
  std::string error;
};

ViewerCliParseResult ParseViewerCommandLine(int argc, char** argv);
std::string BuildViewerHelpText();

class ViewerApp final : public nvvkhl::AppBaseVk
{
public:
  explicit ViewerApp(ViewerCliOptions options);
  ~ViewerApp() override;

  void run();
  void onResize(int width, int height) override;

private:
  ViewerCliOptions m_options;
  nvvk::Context m_vkctx;
  GLFWwindow* m_window{nullptr};
  Engine m_engine;
  std::optional<TrainingSceneRuntime> m_runtime;
  std::unique_ptr<AnimationStateSource> m_animationSource;
  ViewerState m_state;
  ViewerCameraController m_cameraController;
  std::vector<std::string> m_cameraSourceLabels;
  CameraSpec m_autoPreviewCamera;
  int m_selectedCameraSource{0};
  RendererOutputConfig m_lastOutputConfig;
  bool m_outputConfigDirty{true};
  bool m_engineReady{false};
  bool m_appReady{false};
  bool m_resizePending{false};
  int m_pendingResizeWidth{0};
  int m_pendingResizeHeight{0};
  bool m_surfaceCreated{false};
  bool m_pendingScreenshotExport{false};
  bool m_pendingLidarCsvExport{false};
  bool m_pendingHeightScanCsvExport{false};
  bool m_orbitDragActive{false};
  bool m_panDragActive{false};
  std::chrono::steady_clock::time_point m_nextReplayTick{};
  uint64_t m_nextAnimationFrameIndex{0};
  VkSampler m_viewportSampler{VK_NULL_HANDLE};
  VkDescriptorSet m_viewportDescriptor{VK_NULL_HANDLE};
  VkImageView m_registeredImageView{VK_NULL_HANDLE};
  VkImage m_viewportDisplayImage{VK_NULL_HANDLE};
  VkDeviceMemory m_viewportDisplayMemory{VK_NULL_HANDLE};
  VkImageView m_viewportDisplayImageView{VK_NULL_HANDLE};
  VkExtent2D m_viewportDisplayExtent{0, 0};
  bool m_viewportDisplayInitialized{false};
  VkDescriptorPool m_viewportConversionDescriptorPool{VK_NULL_HANDLE};
  VkDescriptorSetLayout m_viewportConversionDescriptorSetLayout{VK_NULL_HANDLE};
  VkDescriptorSet m_viewportConversionDescriptorSet{VK_NULL_HANDLE};
  VkPipelineLayout m_viewportConversionPipelineLayout{VK_NULL_HANDLE};
  VkPipeline m_viewportConversionPipeline{VK_NULL_HANDLE};
  VkImageView m_viewportConversionSourceView{VK_NULL_HANDLE};
  VkImageView m_viewportConversionOutputView{VK_NULL_HANDLE};
  std::string m_statusLine;

  void initialize();
  void shutdown();
  void createWindow();
  void createVulkanContext();
  void initializeEngine();
  void createViewportSampler();
  void createViewportConversionResources();
  void destroyViewportConversionResources();
  void destroyViewportDisplayImage();
  void ensureViewportDisplayImage(VkExtent2D extent);
  void updateViewportConversionDescriptorSet(const ExportedAovTexture& colorAov);
  void convertViewportTexture(const ExportedAovTexture& colorAov);
  void refreshViewportTexture();
  void releaseViewportTexture();

  void runFrame();
  static void framebufferSizeCallback(GLFWwindow* window, int width, int height);
  void deferFramebufferResize(int width, int height);
  void drawUi();
  void drawViewportPanel();
  void drawCameraPanel();
  void drawSensorsPanel();
  void drawReplayPanel();
  void drawExportPanel();
  void drawSensorControls(const char* label, SensorViewerSettings& settings, size_t sensorCount,
                          const std::vector<std::string>& sensorNames);
  void drawFrameToSwapchain();

  void applyCameraInput(bool viewportHovered, ImVec2 viewportSize);
  void resetCameraControllerToSelectedCamera();
  bool syncFramebufferResizeBeforeRender();
  void applyOutputConfigIfDirty();
  void processPendingExports();
  void resetReplayPlaybackClock();
  void advanceReplayForPlayback();
  void stepReplay();
  void resetReplay();
  void exportScreenshot();
  void exportLidarCsv();
  void exportHeightScanCsv();
  void exportDebugBundle();
  std::filesystem::path viewerDebugPath(const std::string& prefix, const std::string& extension) const;
  std::filesystem::path makeUniquePath(std::filesystem::path path) const;
};

} // namespace headless_training
