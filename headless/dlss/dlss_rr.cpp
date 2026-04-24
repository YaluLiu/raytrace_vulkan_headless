#include "dlss_rr.hpp"

#ifndef ENABLE_DLSS_RR
#define ENABLE_DLSS_RR 0
#endif

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <glm/gtc/type_ptr.hpp>

#if ENABLE_DLSS_RR
#include "nvsdk_ngx.h"
#include "nvsdk_ngx_helpers_vk.h"
#include "nvsdk_ngx_helpers_dlssd_vk.h"
#include "nvsdk_ngx_params.h"
#endif

namespace {

std::wstring toWide(const std::string& str)
{
  return std::wstring(str.begin(), str.end());
}

std::string joinStrings(const std::vector<std::string>& values, const char* delimiter)
{
  std::ostringstream oss;
  for(size_t i = 0; i < values.size(); ++i)
  {
    if(i > 0)
    {
      oss << delimiter;
    }
    oss << values[i];
  }
  return oss.str();
}

#if ENABLE_DLSS_RR
using DlssdGetOptimalSettingsCallback = NVSDK_NGX_Result(NVSDK_CONV*)(NVSDK_NGX_Parameter*);

NVSDK_NGX_PerfQuality_Value toNgxPerfQuality(dlss::PerfQuality quality)
{
  switch(quality)
  {
    case dlss::PerfQuality::MaxPerformance:
      return NVSDK_NGX_PerfQuality_Value_MaxPerf;
    case dlss::PerfQuality::Balanced:
      return NVSDK_NGX_PerfQuality_Value_Balanced;
    case dlss::PerfQuality::MaxQuality:
      return NVSDK_NGX_PerfQuality_Value_MaxQuality;
    case dlss::PerfQuality::UltraPerformance:
      return NVSDK_NGX_PerfQuality_Value_UltraPerformance;
    case dlss::PerfQuality::UltraQuality:
      return NVSDK_NGX_PerfQuality_Value_UltraQuality;
    case dlss::PerfQuality::DLAA:
      return NVSDK_NGX_PerfQuality_Value_DLAA;
  }
  return NVSDK_NGX_PerfQuality_Value_Balanced;
}

const char* perfQualityName(dlss::PerfQuality quality)
{
  switch(quality)
  {
    case dlss::PerfQuality::MaxPerformance:
      return "MaxPerformance";
    case dlss::PerfQuality::Balanced:
      return "Balanced";
    case dlss::PerfQuality::MaxQuality:
      return "MaxQuality";
    case dlss::PerfQuality::UltraPerformance:
      return "UltraPerformance";
    case dlss::PerfQuality::UltraQuality:
      return "UltraQuality";
    case dlss::PerfQuality::DLAA:
      return "DLAA";
  }
  return "Balanced";
}

NVSDK_NGX_Result queryDlssdOptimalSettings(NVSDK_NGX_Parameter* params,
                                           unsigned int         targetWidth,
                                           unsigned int         targetHeight,
                                           NVSDK_NGX_PerfQuality_Value quality,
                                           unsigned int*        optimalWidth,
                                           unsigned int*        optimalHeight,
                                           unsigned int*        maxWidth,
                                           unsigned int*        maxHeight,
                                           unsigned int*        minWidth,
                                           unsigned int*        minHeight,
                                           float*               sharpness)
{
  void* callback = nullptr;
  NVSDK_NGX_Parameter_GetVoidPointer(params, NVSDK_NGX_Parameter_DLSSDOptimalSettingsCallback, &callback);
  if(callback == nullptr)
  {
    return NVSDK_NGX_Result_FAIL_OutOfDate;
  }

  NVSDK_NGX_Parameter_SetUI(params, NVSDK_NGX_Parameter_Width, targetWidth);
  NVSDK_NGX_Parameter_SetUI(params, NVSDK_NGX_Parameter_Height, targetHeight);
  NVSDK_NGX_Parameter_SetI(params, NVSDK_NGX_Parameter_PerfQualityValue, quality);
  NVSDK_NGX_Parameter_SetI(params, NVSDK_NGX_Parameter_RTXValue, false);

  const auto callbackFn = reinterpret_cast<DlssdGetOptimalSettingsCallback>(callback);
  const NVSDK_NGX_Result result = callbackFn(params);
  if(result != NVSDK_NGX_Result_Success)
  {
    return result;
  }

  NVSDK_NGX_Parameter_GetUI(params, NVSDK_NGX_Parameter_OutWidth, optimalWidth);
  NVSDK_NGX_Parameter_GetUI(params, NVSDK_NGX_Parameter_OutHeight, optimalHeight);

  *maxWidth  = *optimalWidth;
  *maxHeight = *optimalHeight;
  *minWidth  = *optimalWidth;
  *minHeight = *optimalHeight;

  NVSDK_NGX_Parameter_GetUI(params, NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Max_Render_Width, maxWidth);
  NVSDK_NGX_Parameter_GetUI(params, NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Max_Render_Height, maxHeight);
  NVSDK_NGX_Parameter_GetUI(params, NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Min_Render_Width, minWidth);
  NVSDK_NGX_Parameter_GetUI(params, NVSDK_NGX_Parameter_DLSS_Get_Dynamic_Min_Render_Height, minHeight);
  NVSDK_NGX_Parameter_GetF(params, NVSDK_NGX_Parameter_Sharpness, sharpness);
  return result;
}

std::string wideToAscii(const wchar_t* value)
{
  if(value == nullptr)
  {
    return {};
  }

  std::string out;
  while(*value != L'\0')
  {
    const wchar_t ch = *value++;
    out.push_back((ch >= 0 && ch < 128) ? static_cast<char>(ch) : '?');
  }
  return out;
}
#endif

std::string formatDimensions(const char* label, uint32_t width, uint32_t height)
{
  std::ostringstream oss;
  oss << label << "=" << width << "x" << height;
  return oss.str();
}

std::string formatErrorMessage(const char* prefix, int resultCode)
{
  std::ostringstream oss;
  oss << prefix << " (code=" << resultCode << ")";
#if ENABLE_DLSS_RR
  oss << ", ngx="
      << wideToAscii(GetNGXResultAsString(static_cast<NVSDK_NGX_Result>(resultCode)));
#endif
  return oss.str();
}

bool isValidInputImage(const dlss::ImageInput& image)
{
  return image.image != VK_NULL_HANDLE && image.view != VK_NULL_HANDLE && image.format != VK_FORMAT_UNDEFINED;
}

}  // namespace

namespace dlss {

struct DlssRR::Impl
{
  std::string lastError;
  bool        initialized{false};
  bool        operational{false};

#if ENABLE_DLSS_RR
  VkInstance       instance{VK_NULL_HANDLE};
  VkPhysicalDevice physicalDevice{VK_NULL_HANDLE};
  VkDevice         device{VK_NULL_HANDLE};

  NVSDK_NGX_Handle*    featureHandle{nullptr};
  NVSDK_NGX_Parameter* capabilityParams{nullptr};
  NVSDK_NGX_Parameter* runtimeParams{nullptr};

  uint32_t featureRenderWidth{0};
  uint32_t featureRenderHeight{0};
  uint32_t featureTargetWidth{0};
  uint32_t featureTargetHeight{0};
  PerfQuality featurePerfQuality{PerfQuality::Balanced};

  std::wstring                appDataPathWide;
  std::vector<std::wstring>   featureSearchPathsWide;
  std::vector<const wchar_t*> featureSearchPathsPtr;
#endif
};

DlssRR::DlssRR()
    : m_impl(std::make_unique<Impl>())
{
}

DlssRR::~DlssRR()
{
  shutdown();
}

DlssRR::DlssRR(DlssRR&& other) noexcept
    : m_impl(std::move(other.m_impl))
{
}

DlssRR& DlssRR::operator=(DlssRR&& other) noexcept
{
  if(this != &other)
  {
    shutdown();
    m_impl = std::move(other.m_impl);
  }
  return *this;
}

bool DlssRR::initialize(const InitInputs& inputs)
{
  shutdown();
  m_impl->lastError.clear();

#if !ENABLE_DLSS_RR
  (void)inputs;
  m_impl->lastError = "DLSS-RR is disabled at build time";
  return false;
#else
  if(inputs.instance == VK_NULL_HANDLE || inputs.physicalDevice == VK_NULL_HANDLE || inputs.device == VK_NULL_HANDLE)
  {
    m_impl->lastError = "DLSS-RR init failed: invalid Vulkan instance/device";
    return false;
  }

  m_impl->instance       = inputs.instance;
  m_impl->physicalDevice = inputs.physicalDevice;
  m_impl->device         = inputs.device;

  std::filesystem::path appDataPath = inputs.applicationDataPath.empty()
                                          ? (std::filesystem::current_path() / "output" / "ngx")
                                          : std::filesystem::path(inputs.applicationDataPath);
  std::error_code ec;
  std::filesystem::create_directories(appDataPath, ec);
  if(ec)
  {
    m_impl->lastError =
        "DLSS-RR init failed: unable to create app data path '" + appDataPath.string() + "' (" + ec.message() + ")";
    return false;
  }
  m_impl->appDataPathWide = toWide(appDataPath.lexically_normal().string());

  std::vector<std::string> searchPaths = inputs.featureSearchPaths;

  if(const char* envPath = std::getenv("DLSS_RR_FEATURE_PATH"))
  {
    if(*envPath != '\0')
    {
      searchPaths.emplace_back(envPath);
    }
  }

  if(const char* sdkRoot = std::getenv("DLSS_SDK_ROOT"))
  {
    if(*sdkRoot != '\0')
    {
      searchPaths.emplace_back((std::filesystem::path(sdkRoot) / "lib" / "Linux_x86_64" / "rel").string());
    }
  }

#if defined(DLSS_RR_DEFAULT_NGX_PATH)
  if(std::char_traits<char>::length(DLSS_RR_DEFAULT_NGX_PATH) != 0)
  {
    searchPaths.emplace_back(DLSS_RR_DEFAULT_NGX_PATH);
  }
#endif

  std::vector<std::string> normalizedSearchPaths;
  normalizedSearchPaths.reserve(searchPaths.size());
  for(const auto& path : searchPaths)
  {
    if(path.empty())
    {
      continue;
    }

    const std::string normalized = std::filesystem::path(path).lexically_normal().string();
    if(std::find(normalizedSearchPaths.begin(), normalizedSearchPaths.end(), normalized) == normalizedSearchPaths.end())
    {
      normalizedSearchPaths.emplace_back(normalized);
    }
  }

  m_impl->featureSearchPathsWide.clear();
  m_impl->featureSearchPathsWide.reserve(normalizedSearchPaths.size());
  for(const auto& path : normalizedSearchPaths)
  {
    if(path.empty())
    {
      continue;
    }
    m_impl->featureSearchPathsWide.emplace_back(toWide(path));
  }

  m_impl->featureSearchPathsPtr.clear();
  m_impl->featureSearchPathsPtr.reserve(m_impl->featureSearchPathsWide.size());
  for(const auto& path : m_impl->featureSearchPathsWide)
  {
    m_impl->featureSearchPathsPtr.emplace_back(path.c_str());
  }

  NVSDK_NGX_FeatureCommonInfo featureInfo{};
  if(!m_impl->featureSearchPathsPtr.empty())
  {
    featureInfo.PathListInfo.Path   = m_impl->featureSearchPathsPtr.data();
    featureInfo.PathListInfo.Length = static_cast<unsigned int>(m_impl->featureSearchPathsPtr.size());
  }

  const char* projectId = std::getenv("DLSS_RR_PROJECT_ID");
  if(projectId == nullptr || *projectId == '\0')
  {
    projectId = "de4f7bf3-2e96-4b85-a760-5a3357d63a8b";
  }

  NVSDK_NGX_Result initResult =
      NVSDK_NGX_VULKAN_Init_with_ProjectID(projectId, NVSDK_NGX_ENGINE_TYPE_CUSTOM, "raytrace_vulkan_headless",
                                           m_impl->appDataPathWide.c_str(), m_impl->instance, m_impl->physicalDevice,
                                           m_impl->device, vkGetInstanceProcAddr, vkGetDeviceProcAddr,
                                           featureInfo.PathListInfo.Length > 0 ? &featureInfo : nullptr);

  if(initResult != NVSDK_NGX_Result_Success)
  {
    const std::string pathSummary =
        normalizedSearchPaths.empty() ? "<default-app-path-only>" : joinStrings(normalizedSearchPaths, ";");
    m_impl->lastError = formatErrorMessage("DLSS-RR init failed", static_cast<int>(initResult))
                        + ", appDataPath=" + appDataPath.string() + ", featurePaths=" + pathSummary;
    return false;
  }

  m_impl->initialized = true;

  NVSDK_NGX_Result capResult = NVSDK_NGX_VULKAN_GetCapabilityParameters(&m_impl->capabilityParams);
  if(capResult != NVSDK_NGX_Result_Success || m_impl->capabilityParams == nullptr)
  {
    m_impl->lastError = formatErrorMessage("DLSS-RR capability query failed", static_cast<int>(capResult));
    return false;
  }

  int available = 0;
  NVSDK_NGX_Parameter_GetI(m_impl->capabilityParams, NVSDK_NGX_Parameter_SuperSamplingDenoising_Available, &available);
  if(available == 0)
  {
    m_impl->lastError = "DLSS-RR capability unavailable on this GPU/driver";
    return false;
  }

  int needsUpdatedDriver = 0;
  NVSDK_NGX_Parameter_GetI(m_impl->capabilityParams, NVSDK_NGX_Parameter_SuperSamplingDenoising_NeedsUpdatedDriver, &needsUpdatedDriver);
  if(needsUpdatedDriver != 0)
  {
    m_impl->lastError = "DLSS-RR requires a newer NVIDIA driver";
    return false;
  }

  NVSDK_NGX_Result allocResult = NVSDK_NGX_VULKAN_AllocateParameters(&m_impl->runtimeParams);
  if(allocResult != NVSDK_NGX_Result_Success || m_impl->runtimeParams == nullptr)
  {
    m_impl->lastError = formatErrorMessage("DLSS-RR parameter allocation failed", static_cast<int>(allocResult));
    return false;
  }

  m_impl->operational = true;
  return true;
#endif
}

void DlssRR::shutdown()
{
  if(!m_impl)
  {
    return;
  }

#if ENABLE_DLSS_RR
  if(m_impl->featureHandle != nullptr)
  {
    NVSDK_NGX_VULKAN_ReleaseFeature(m_impl->featureHandle);
    m_impl->featureHandle = nullptr;
  }

  if(m_impl->runtimeParams != nullptr)
  {
    NVSDK_NGX_VULKAN_DestroyParameters(m_impl->runtimeParams);
    m_impl->runtimeParams = nullptr;
  }

  if(m_impl->capabilityParams != nullptr)
  {
    NVSDK_NGX_VULKAN_DestroyParameters(m_impl->capabilityParams);
    m_impl->capabilityParams = nullptr;
  }

  if(m_impl->initialized && m_impl->device != VK_NULL_HANDLE)
  {
    NVSDK_NGX_VULKAN_Shutdown1(m_impl->device);
  }

  m_impl->featureRenderWidth  = 0;
  m_impl->featureRenderHeight = 0;
  m_impl->featureTargetWidth  = 0;
  m_impl->featureTargetHeight = 0;
  m_impl->featurePerfQuality  = PerfQuality::Balanced;
#endif

  m_impl->initialized = false;
  m_impl->operational = false;
}

bool DlssRR::queryOptimalSettings(uint32_t targetWidth, uint32_t targetHeight, PerfQuality quality, OptimalSettings& settings)
{
  settings = {};

#if !ENABLE_DLSS_RR
  (void)targetWidth;
  (void)targetHeight;
  (void)quality;
  m_impl->lastError = "DLSS-RR is disabled at build time";
  return false;
#else
  if(!m_impl->operational || !m_impl->initialized || m_impl->capabilityParams == nullptr)
  {
    m_impl->lastError = "DLSS-RR optimal settings query failed: feature is not operational";
    return false;
  }

  if(targetWidth == 0 || targetHeight == 0)
  {
    m_impl->lastError = "DLSS-RR optimal settings query failed: invalid target dimensions";
    return false;
  }

  unsigned int optimalWidth = 0;
  unsigned int optimalHeight = 0;
  unsigned int maxWidth = 0;
  unsigned int maxHeight = 0;
  unsigned int minWidth = 0;
  unsigned int minHeight = 0;
  float        sharpness = 0.0f;

  const NVSDK_NGX_Result result =
      queryDlssdOptimalSettings(m_impl->capabilityParams, targetWidth, targetHeight, toNgxPerfQuality(quality),
                                &optimalWidth, &optimalHeight, &maxWidth, &maxHeight, &minWidth, &minHeight, &sharpness);
  if(result != NVSDK_NGX_Result_Success || optimalWidth == 0 || optimalHeight == 0)
  {
    m_impl->lastError = formatErrorMessage("DLSS-RR optimal settings query failed", static_cast<int>(result)) + ", "
                        + formatDimensions("target", targetWidth, targetHeight) + ", quality=" + perfQualityName(quality);
    return false;
  }

  settings.renderWidth     = optimalWidth;
  settings.renderHeight    = optimalHeight;
  settings.minRenderWidth  = minWidth;
  settings.minRenderHeight = minHeight;
  settings.maxRenderWidth  = maxWidth;
  settings.maxRenderHeight = maxHeight;
  settings.sharpness       = sharpness;
  return true;
#endif
}

bool DlssRR::evaluate(const EvaluateInputs& inputs)
{
#if !ENABLE_DLSS_RR
  (void)inputs;
  m_impl->lastError = "DLSS-RR is disabled at build time";
  return false;
#else
  if(!m_impl->operational || !m_impl->initialized)
  {
    if(m_impl->lastError.empty())
    {
      m_impl->lastError = "DLSS-RR evaluate skipped: feature is not operational";
    }
    return false;
  }

  if(inputs.cmd == VK_NULL_HANDLE || inputs.renderWidth == 0 || inputs.renderHeight == 0 || inputs.targetWidth == 0
     || inputs.targetHeight == 0)
  {
    m_impl->lastError = "DLSS-RR evaluate failed: invalid command buffer or dimensions";
    return false;
  }

  if(!isValidInputImage(inputs.color) || !isValidInputImage(inputs.output) || !isValidInputImage(inputs.depth)
     || !isValidInputImage(inputs.motionVectors) || !isValidInputImage(inputs.diffuseAlbedo)
     || !isValidInputImage(inputs.specularAlbedo) || !isValidInputImage(inputs.normalsRoughness))
  {
    m_impl->lastError = "DLSS-RR evaluate failed: missing required input image";
    return false;
  }

  if(m_impl->runtimeParams == nullptr)
  {
    m_impl->lastError = "DLSS-RR evaluate failed: runtime parameter map is null";
    return false;
  }

  if(m_impl->featureHandle == nullptr || m_impl->featureRenderWidth != inputs.renderWidth
     || m_impl->featureRenderHeight != inputs.renderHeight || m_impl->featureTargetWidth != inputs.targetWidth
     || m_impl->featureTargetHeight != inputs.targetHeight || m_impl->featurePerfQuality != inputs.perfQuality)
  {
    if(m_impl->featureHandle != nullptr)
    {
      NVSDK_NGX_VULKAN_ReleaseFeature(m_impl->featureHandle);
      m_impl->featureHandle = nullptr;
    }

    NVSDK_NGX_DLSSD_Create_Params createParams{};
    createParams.InDenoiseMode          = NVSDK_NGX_DLSS_Denoise_Mode_DLUnified;
    createParams.InRoughnessMode        = NVSDK_NGX_DLSS_Roughness_Mode_Packed;
    createParams.InUseHWDepth           = NVSDK_NGX_DLSS_Depth_Type_Linear;
    createParams.InWidth                = inputs.renderWidth;
    createParams.InHeight               = inputs.renderHeight;
    createParams.InTargetWidth          = inputs.targetWidth;
    createParams.InTargetHeight         = inputs.targetHeight;
    createParams.InPerfQualityValue     = toNgxPerfQuality(inputs.perfQuality);
    createParams.InFeatureCreateFlags   = NVSDK_NGX_DLSS_Feature_Flags_IsHDR | NVSDK_NGX_DLSS_Feature_Flags_MVLowRes;
    createParams.InEnableOutputSubrects = false;

    NVSDK_NGX_Result createResult = NGX_VULKAN_CREATE_DLSSD_EXT1(m_impl->device, inputs.cmd, 1, 1, &m_impl->featureHandle,
                                                                 m_impl->runtimeParams, &createParams);

    if(createResult != NVSDK_NGX_Result_Success || m_impl->featureHandle == nullptr)
    {
      m_impl->lastError = formatErrorMessage("DLSS-RR feature creation failed", static_cast<int>(createResult)) + ", "
                          + formatDimensions("render", inputs.renderWidth, inputs.renderHeight) + ", "
                          + formatDimensions("target", inputs.targetWidth, inputs.targetHeight) + ", quality="
                          + perfQualityName(inputs.perfQuality);
      return false;
    }

    m_impl->featureRenderWidth  = inputs.renderWidth;
    m_impl->featureRenderHeight = inputs.renderHeight;
    m_impl->featureTargetWidth  = inputs.targetWidth;
    m_impl->featureTargetHeight = inputs.targetHeight;
    m_impl->featurePerfQuality  = inputs.perfQuality;
  }

  VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

  auto makeImageResource = [&](const ImageInput& image, uint32_t width, uint32_t height, bool readWrite) {
    return NVSDK_NGX_Create_ImageView_Resource_VK(image.view, image.image, range, image.format, width, height, readWrite);
  };

  NVSDK_NGX_Resource_VK colorResource =
      makeImageResource(inputs.color, inputs.renderWidth, inputs.renderHeight, false);
  NVSDK_NGX_Resource_VK outputResource =
      makeImageResource(inputs.output, inputs.targetWidth, inputs.targetHeight, true);
  NVSDK_NGX_Resource_VK depthResource =
      makeImageResource(inputs.depth, inputs.renderWidth, inputs.renderHeight, false);
  NVSDK_NGX_Resource_VK motionResource =
      makeImageResource(inputs.motionVectors, inputs.renderWidth, inputs.renderHeight, false);
  NVSDK_NGX_Resource_VK diffuseAlbedoResource =
      makeImageResource(inputs.diffuseAlbedo, inputs.renderWidth, inputs.renderHeight, false);
  NVSDK_NGX_Resource_VK specularAlbedoResource =
      makeImageResource(inputs.specularAlbedo, inputs.renderWidth, inputs.renderHeight, false);
  NVSDK_NGX_Resource_VK normalRoughnessResource =
      makeImageResource(inputs.normalsRoughness, inputs.renderWidth, inputs.renderHeight, false);
  NVSDK_NGX_Resource_VK specularHitDistanceResource{};
  const bool            hasSpecularHitDistance = isValidInputImage(inputs.specularHitDistance);
  if(hasSpecularHitDistance)
  {
    specularHitDistanceResource = makeImageResource(inputs.specularHitDistance, inputs.renderWidth, inputs.renderHeight, false);
  }

  m_impl->runtimeParams->Reset();

  NVSDK_NGX_VK_DLSSD_Eval_Params evalParams{};
  evalParams.pInColor               = &colorResource;
  evalParams.pInOutput              = &outputResource;
  evalParams.pInDepth               = &depthResource;
  evalParams.pInMotionVectors       = &motionResource;
  evalParams.pInDiffuseAlbedo       = &diffuseAlbedoResource;
  evalParams.pInSpecularAlbedo      = &specularAlbedoResource;
  evalParams.pInNormals             = &normalRoughnessResource;
  evalParams.pInRoughness           = &normalRoughnessResource;  // Match NVIDIA sample path for packed roughness.
  evalParams.pInSpecularHitDistance = hasSpecularHitDistance ? &specularHitDistanceResource : nullptr;

  evalParams.InJitterOffsetX           = inputs.jitterX;
  evalParams.InJitterOffsetY           = inputs.jitterY;
  evalParams.InRenderSubrectDimensions = {inputs.renderWidth, inputs.renderHeight};
  evalParams.InReset                   = inputs.reset ? 1 : 0;
  evalParams.InMVScaleX                = inputs.mvScaleX;
  evalParams.InMVScaleY                = inputs.mvScaleY;
  evalParams.InPreExposure             = 1.0f;
  evalParams.InExposureScale           = 1.0f;
  evalParams.InFrameTimeDeltaInMsec    = std::max(inputs.frameTimeMs, 0.0f);
  evalParams.InToneMapperType          = NVSDK_NGX_TONEMAPPER_STRING;
  evalParams.pInWorldToViewMatrix      = const_cast<float*>(glm::value_ptr(inputs.worldToView));
  evalParams.pInViewToClipMatrix       = const_cast<float*>(glm::value_ptr(inputs.viewToClip));

  NVSDK_NGX_Result evaluateResult =
      NGX_VULKAN_EVALUATE_DLSSD_EXT(inputs.cmd, m_impl->featureHandle, m_impl->runtimeParams, &evalParams);
  if(evaluateResult != NVSDK_NGX_Result_Success)
  {
    m_impl->lastError = formatErrorMessage("DLSS-RR evaluate failed", static_cast<int>(evaluateResult));
    return false;
  }

  return true;
#endif
}

bool DlssRR::isApiEnabled() const
{
  return ENABLE_DLSS_RR != 0;
}

bool DlssRR::isOperational() const
{
  return m_impl->operational;
}

const std::string& DlssRR::getLastError() const
{
  return m_impl->lastError;
}

}  // namespace dlss
