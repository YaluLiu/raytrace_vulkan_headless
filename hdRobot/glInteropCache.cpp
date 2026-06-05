#ifdef _WIN32
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#include <windows.h>
#endif

#include "glInteropCache.h"

#ifndef _WIN32
#include <GL/glx.h>
#endif

#include <cstdint>
#include <iostream>
#include <mutex>
#include <type_traits>
#include <unordered_map>

#ifdef _WIN32
#include <vulkan/vulkan_win32.h>
#else
#include <unistd.h>
#endif

namespace {
struct CacheKey {
  VkDevice device{VK_NULL_HANDLE};
  VkImage image{VK_NULL_HANDLE};
  VkDeviceMemory memory{VK_NULL_HANDLE};
  VkDeviceSize memoryOffset{0};
  VkDeviceSize memorySize{0};
  VkFormat format{VK_FORMAT_UNDEFINED};
  VkExtent2D extent{0, 0};

  bool operator==(const CacheKey &other) const {
    return device == other.device && image == other.image &&
           memory == other.memory && memoryOffset == other.memoryOffset &&
           memorySize == other.memorySize && format == other.format &&
           extent.width == other.extent.width &&
           extent.height == other.extent.height;
  }
};

struct CacheEntry {
  GLuint memoryObject{0};
  GLuint texture{0};
#ifdef _WIN32
  HANDLE win32Handle{nullptr};
#endif
};

template <typename Handle> std::uint64_t HandleValue(Handle handle) {
  if constexpr (std::is_pointer_v<Handle>) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(handle));
  } else {
    return static_cast<std::uint64_t>(handle);
  }
}

void HashCombine(std::size_t &seed, std::uint64_t value) {
  seed ^= static_cast<std::size_t>(value) + 0x9e3779b97f4a7c15ull +
          (seed << 6) + (seed >> 2);
}

struct CacheKeyHash {
  std::size_t operator()(const CacheKey &key) const {
    std::size_t seed = 0;
    HashCombine(seed, HandleValue(key.device));
    HashCombine(seed, HandleValue(key.image));
    HashCombine(seed, HandleValue(key.memory));
    HashCombine(seed, static_cast<std::uint64_t>(key.memoryOffset));
    HashCombine(seed, static_cast<std::uint64_t>(key.memorySize));
    HashCombine(seed, static_cast<std::uint64_t>(key.format));
    HashCombine(seed, key.extent.width);
    HashCombine(seed, key.extent.height);
    return seed;
  }
};

GLint GetGlInternalFormat(VkFormat format) {
  switch (format) {
  case VK_FORMAT_R32G32B32A32_SFLOAT:
    return GL_RGBA32F;
  case VK_FORMAT_R32G32_SFLOAT:
    return GL_RG32F;
  case VK_FORMAT_R32_SFLOAT:
    return GL_R32F;
  case VK_FORMAT_R32_SINT:
    return GL_R32I;
  default:
    return 0;
  }
}

#ifdef _WIN32
void *GetGlProcAddress(const char *name) {
  PROC proc = wglGetProcAddress(name);
  if (proc != nullptr && proc != reinterpret_cast<PROC>(1) &&
      proc != reinterpret_cast<PROC>(2) && proc != reinterpret_cast<PROC>(3) &&
      proc != reinterpret_cast<PROC>(-1)) {
    return reinterpret_cast<void *>(proc);
  }

  HMODULE opengl = GetModuleHandleA("opengl32.dll");
  if (opengl == nullptr) {
    opengl = LoadLibraryA("opengl32.dll");
  }
  return opengl != nullptr ? reinterpret_cast<void *>(GetProcAddress(opengl, name))
                           : nullptr;
}
#else
void *GetGlProcAddress(const char *name) {
  return reinterpret_cast<void *>(
      glXGetProcAddressARB(reinterpret_cast<const GLubyte *>(name)));
}
#endif

bool RequiredGlExtensionsAvailable() {
  if (!has_GL_VERSION_4_5) {
    std::cerr << "[GlInteropCache] OpenGL 4.5 functions required for "
                 "DSA texture/FBO copies are unavailable"
              << std::endl;
    return false;
  }

  if (!has_GL_EXT_memory_object) {
    std::cerr << "[GlInteropCache] GL_EXT_memory_object is unavailable"
              << std::endl;
    return false;
  }

#ifdef _WIN32
  if (!has_GL_EXT_memory_object_win32) {
    std::cerr << "[GlInteropCache] GL_EXT_memory_object_win32 is "
                 "unavailable"
              << std::endl;
    return false;
  }
#else
  if (!has_GL_EXT_memory_object_fd) {
    std::cerr << "[GlInteropCache] GL_EXT_memory_object_fd is "
                 "unavailable"
              << std::endl;
    return false;
  }
#endif

  return true;
}

bool EnsureGlFunctionsAvailable() {
  static std::mutex loaderMutex;
  static bool loaded = false;

  std::lock_guard<std::mutex> lock(loaderMutex);
  if (!loaded) {
    load_GL(GetGlProcAddress);
    if (!RequiredGlExtensionsAvailable()) {
      return false;
    }
    loaded = true;
  }

  return RequiredGlExtensionsAvailable();
}

void ClearGlErrors() {
  while (glGetError() != GL_NO_ERROR) {
  }
}

bool CheckGlError(const char *operation) {
  const GLenum error = glGetError();
  if (error == GL_NO_ERROR) {
    return true;
  }

  std::cerr << "[GlInteropCache] " << operation
            << " failed with GL error 0x" << std::hex << error << std::dec
            << std::endl;
  return false;
}

void DestroyEntry(CacheEntry &entry) {
  if (entry.texture != 0) {
    glDeleteTextures(1, &entry.texture);
    entry.texture = 0;
  }
  if (entry.memoryObject != 0) {
    glDeleteMemoryObjectsEXT(1, &entry.memoryObject);
    entry.memoryObject = 0;
  }
#ifdef _WIN32
  if (entry.win32Handle != nullptr) {
    CloseHandle(entry.win32Handle);
    entry.win32Handle = nullptr;
  }
#endif
}

bool SetDedicatedMemoryFlag(const ExportedAovTexture &texture,
                            GLuint memoryObject) {
  if (!texture.dedicatedMemory) {
    return true;
  }

  const GLint dedicated = GL_TRUE;
  ClearGlErrors();
  glMemoryObjectParameterivEXT(memoryObject, GL_DEDICATED_MEMORY_OBJECT_EXT,
                               &dedicated);
  return CheckGlError("glMemoryObjectParameterivEXT");
}

#ifdef _WIN32
bool ImportVulkanMemoryToGl(const ExportedAovTexture &texture,
                            CacheEntry &entry) {
  const auto getMemoryWin32Handle =
      reinterpret_cast<PFN_vkGetMemoryWin32HandleKHR>(
          vkGetDeviceProcAddr(texture.device, "vkGetMemoryWin32HandleKHR"));
  if (getMemoryWin32Handle == nullptr) {
    std::cerr << "[GlInteropCache] Missing vkGetMemoryWin32HandleKHR"
              << std::endl;
    return false;
  }

  VkMemoryGetWin32HandleInfoKHR handleInfo{
      VK_STRUCTURE_TYPE_MEMORY_GET_WIN32_HANDLE_INFO_KHR};
  handleInfo.memory = texture.memory;
  handleInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_WIN32_BIT;

  HANDLE handle = nullptr;
  const VkResult result =
      getMemoryWin32Handle(texture.device, &handleInfo, &handle);
  if (result != VK_SUCCESS || handle == nullptr) {
    std::cerr << "[GlInteropCache] vkGetMemoryWin32HandleKHR failed: "
              << result << std::endl;
    return false;
  }

  ClearGlErrors();
  glImportMemoryWin32HandleEXT(entry.memoryObject,
                               static_cast<GLuint64>(texture.memorySize),
                               GL_HANDLE_TYPE_OPAQUE_WIN32_EXT, handle);
  if (!CheckGlError("glImportMemoryWin32HandleEXT")) {
    CloseHandle(handle);
    return false;
  }

  entry.win32Handle = handle;
  return true;
}
#else
bool ImportVulkanMemoryToGl(const ExportedAovTexture &texture,
                            GLuint memoryObject) {
  const auto getMemoryFd = reinterpret_cast<PFN_vkGetMemoryFdKHR>(
      vkGetDeviceProcAddr(texture.device, "vkGetMemoryFdKHR"));
  if (getMemoryFd == nullptr) {
    std::cerr << "[GlInteropCache] Missing vkGetMemoryFdKHR"
              << std::endl;
    return false;
  }

  VkMemoryGetFdInfoKHR fdInfo{VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR};
  fdInfo.memory = texture.memory;
  fdInfo.handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;

  int fd = -1;
  const VkResult result = getMemoryFd(texture.device, &fdInfo, &fd);
  if (result != VK_SUCCESS || fd < 0) {
    std::cerr << "[GlInteropCache] vkGetMemoryFdKHR failed: " << result
              << std::endl;
    return false;
  }

  ClearGlErrors();
  glImportMemoryFdEXT(memoryObject, static_cast<GLuint64>(texture.memorySize),
                      GL_HANDLE_TYPE_OPAQUE_FD_EXT, fd);
  if (CheckGlError("glImportMemoryFdEXT")) {
    return true;
  }

  close(fd);
  return false;
}
#endif

CacheKey MakeCacheKey(const ExportedAovTexture &texture) {
  CacheKey key;
  key.device = texture.device;
  key.image = texture.image;
  key.memory = texture.memory;
  key.memoryOffset = texture.memoryOffset;
  key.memorySize = texture.memorySize;
  key.format = texture.format;
  key.extent = texture.extent;
  return key;
}
} // namespace

PXR_NAMESPACE_OPEN_SCOPE

namespace hdrobot {

struct GlInteropCache::Impl {
  std::mutex mutex;
  std::unordered_map<CacheKey, CacheEntry, CacheKeyHash> cache;
};

GlInteropCache::GlInteropCache()
    : _impl(std::make_unique<Impl>()) {}

GlInteropCache::~GlInteropCache() { Clear(); }

GLuint GlInteropCache::GetOrImportSourceGlTexture(
    const ExportedAovTexture &texture) {
  if (texture.device == VK_NULL_HANDLE || texture.image == VK_NULL_HANDLE ||
      texture.memory == VK_NULL_HANDLE || texture.memorySize == 0 ||
      texture.extent.width == 0 || texture.extent.height == 0) {
    std::cerr << "[GlInteropCache] Invalid AOV texture export metadata"
              << std::endl;
    return 0;
  }

  const GLint internalFormat = GetGlInternalFormat(texture.format);
  if (internalFormat == 0) {
    std::cerr << "[GlInteropCache] Unsupported AOV VkFormat: "
              << texture.format << std::endl;
    return 0;
  }

  std::lock_guard<std::mutex> lock(_impl->mutex);
  const CacheKey key = MakeCacheKey(texture);
  const auto it = _impl->cache.find(key);
  if (it != _impl->cache.end()) {
    return it->second.texture;
  }

  if (!EnsureGlFunctionsAvailable()) {
    return 0;
  }

  CacheEntry entry;
  ClearGlErrors();
  glCreateMemoryObjectsEXT(1, &entry.memoryObject);
  if (entry.memoryObject == 0 ||
      !CheckGlError("glCreateMemoryObjectsEXT")) {
    return 0;
  }

  if (!SetDedicatedMemoryFlag(texture, entry.memoryObject)) {
    DestroyEntry(entry);
    return 0;
  }

#ifdef _WIN32
  if (!ImportVulkanMemoryToGl(texture, entry)) {
    DestroyEntry(entry);
    return 0;
  }
#else
  if (!ImportVulkanMemoryToGl(texture, entry.memoryObject)) {
    DestroyEntry(entry);
    return 0;
  }
#endif

  ClearGlErrors();
  glCreateTextures(GL_TEXTURE_2D, 1, &entry.texture);
  if (entry.texture != 0 && CheckGlError("glCreateTextures")) {
    glTextureStorageMem2DEXT(
        entry.texture, 1, static_cast<GLenum>(internalFormat),
        static_cast<GLsizei>(texture.extent.width),
        static_cast<GLsizei>(texture.extent.height), entry.memoryObject,
        static_cast<GLuint64>(texture.memoryOffset));
    glTextureParameteri(entry.texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTextureParameteri(entry.texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTextureParameteri(entry.texture, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTextureParameteri(entry.texture, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }

  if (entry.texture == 0 || !CheckGlError("glTextureStorageMem2DEXT")) {
    if (entry.texture == 0) {
      std::cerr << "[GlInteropCache] glCreateTextures returned 0"
                << std::endl;
    }
    DestroyEntry(entry);
    return 0;
  }

  _impl->cache.emplace(key, entry);
  return entry.texture;
}

void GlInteropCache::Evict(const ExportedAovTexture &texture) {
  std::lock_guard<std::mutex> lock(_impl->mutex);
  const CacheKey key = MakeCacheKey(texture);
  const auto it = _impl->cache.find(key);
  if (it == _impl->cache.end()) {
    return;
  }

  DestroyEntry(it->second);
  _impl->cache.erase(it);
}

void GlInteropCache::Clear() {
  std::lock_guard<std::mutex> lock(_impl->mutex);
  for (auto &cacheItem : _impl->cache) {
    DestroyEntry(cacheItem.second);
  }
  _impl->cache.clear();
}

} // namespace hdrobot

PXR_NAMESPACE_CLOSE_SCOPE
