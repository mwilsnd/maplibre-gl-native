#include <mbgl/vulkan/renderer_backend.hpp>
#include <mbgl/vulkan/context.hpp>
#include <mbgl/vulkan/renderable_resource.hpp>

#include <mbgl/gfx/backend_scope.hpp>
#include <mbgl/gfx/shader_registry.hpp>
#include <mbgl/shaders/shader_source.hpp>
#include <mbgl/util/logging.hpp>

#include <mbgl/shaders/vulkan/shader_group.hpp>
#include <mbgl/shaders/vulkan/background.hpp>
#include <mbgl/shaders/vulkan/circle.hpp>
#include <mbgl/shaders/vulkan/clipping_mask.hpp>
#include <mbgl/shaders/vulkan/collision.hpp>
#include <mbgl/shaders/vulkan/custom_geometry.hpp>
#include <mbgl/shaders/vulkan/custom_symbol_icon.hpp>
#include <mbgl/shaders/vulkan/debug.hpp>
#include <mbgl/shaders/vulkan/fill.hpp>
#include <mbgl/shaders/vulkan/fill_extrusion.hpp>
#include <mbgl/shaders/vulkan/heatmap.hpp>
#include <mbgl/shaders/vulkan/heatmap_texture.hpp>
#include <mbgl/shaders/vulkan/hillshade.hpp>
#include <mbgl/shaders/vulkan/hillshade_prepare.hpp>
#include <mbgl/shaders/vulkan/line.hpp>
#include <mbgl/shaders/vulkan/location_indicator.hpp>
#include <mbgl/shaders/vulkan/raster.hpp>
#include <mbgl/shaders/vulkan/symbol.hpp>
#include <mbgl/shaders/vulkan/widevector.hpp>

#include <cassert>
#include <string>

#ifdef ENABLE_VULKAN_VALIDATION
#include <vulkan/vulkan_to_string.hpp>
#endif

#ifdef ENABLE_VMA_DEBUG

#define VMA_DEBUG_MARGIN 16
#define VMA_DEBUG_DETECT_CORRUPTION 1
#define VMA_DEBUG_INITIALIZE_ALLOCATIONS 1

#define VMA_LEAK_LOG_FORMAT(format, ...)              \
    {                                                 \
        char buffer[4096];                            \
        sprintf(buffer, format, __VA_ARGS__);         \
        mbgl::Log::Info(mbgl::Event::Render, buffer); \
    }

#endif

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#ifdef ENABLE_RENDERDOC_FRAME_CAPTURE

#ifdef _WIN32
#include <windows.h>
#endif

#include "renderdoc_app.h"

static struct {
    RENDERDOC_API_1_1_2* api = nullptr;
    bool loop = false;
    int32_t frameDelay = 0;
    uint32_t frameCaptureCount = 0;
} g_rdoc;

#endif

namespace mbgl {
namespace vulkan {

template <typename T, typename F>
static bool checkAvailability(const std::vector<T>& availableValues,
                              const std::vector<const char*>& requiredValues,
                              const F& getter) {
    for (const auto& requiredValue : requiredValues) {
        bool found = false;
        for (const auto& availableValue : availableValues) {
            if (strcmp(requiredValue, getter(availableValue)) == 0) {
                found = true;
                break;
            }
        }

        if (!found) return false;
    }

    return true;
}

RendererBackend::RendererBackend(const gfx::ContextMode contextMode_)
    : gfx::RendererBackend(contextMode_),
      allocator(nullptr) {}

RendererBackend::~RendererBackend() {
    destroyResources();
}

std::unique_ptr<gfx::Context> RendererBackend::createContext() {
    return std::make_unique<vulkan::Context>(*this);
}

std::vector<const char*> RendererBackend::getLayers() {
    return {
#ifdef ENABLE_VULKAN_VALIDATION
        "VK_LAYER_KHRONOS_validation",
#endif

#ifdef _WIN32
        "VK_LAYER_LUNARG_monitor",
#endif
    };
}

std::vector<const char*> RendererBackend::getInstanceExtensions() {
    return {
#ifdef __APPLE__
        "VK_KHR_portability_enumeration"
#endif
    };
}

std::vector<const char*> RendererBackend::getDeviceExtensions() {
    auto extensions = getDefaultRenderable().getResource<SurfaceRenderableResource>().getDeviceExtensions();

#ifdef __APPLE__
    extensions.push_back("VK_KHR_portability_subset");
#endif

    return extensions;
}

std::vector<const char*> RendererBackend::getDebugExtensions() {
    std::vector<const char*> extensions;

    const auto& availableExtensions = vk::enumerateInstanceExtensionProperties(nullptr, dispatcher);

    debugUtilsEnabled = checkAvailability(
        availableExtensions, {VK_EXT_DEBUG_UTILS_EXTENSION_NAME}, [](const vk::ExtensionProperties& value) {
            return value.extensionName.data();
        });

    if (debugUtilsEnabled) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    } else {
        // check for VK_EXT_debug_report (deprecated)
        bool debugReportAvailable = checkAvailability(
            availableExtensions, {VK_EXT_DEBUG_REPORT_EXTENSION_NAME}, [](const vk::ExtensionProperties& value) {
                return value.extensionName.data();
            });

        if (debugReportAvailable) {
            extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
        } else {
            mbgl::Log::Error(mbgl::Event::Render, "No debugging extension available");
        }
    }

    return extensions;
}

void RendererBackend::beginDebugLabel([[maybe_unused]] const vk::CommandBuffer& buffer,
                                      [[maybe_unused]] const char* name,
                                      [[maybe_unused]] const std::array<float, 4>& color) const {
#ifdef ENABLE_VULKAN_VALIDATION
    if (!debugUtilsEnabled) return;
    buffer.beginDebugUtilsLabelEXT(vk::DebugUtilsLabelEXT().setPLabelName(name).setColor(color), dispatcher);
#endif
}

void RendererBackend::endDebugLabel([[maybe_unused]] const vk::CommandBuffer& buffer) const {
#ifdef ENABLE_VULKAN_VALIDATION
    if (!debugUtilsEnabled) return;
    buffer.endDebugUtilsLabelEXT(dispatcher);
#endif
}

void RendererBackend::insertDebugLabel([[maybe_unused]] const vk::CommandBuffer& buffer,
                                       [[maybe_unused]] const char* name) const {
#ifdef ENABLE_VULKAN_VALIDATION
    if (!debugUtilsEnabled) return;
    buffer.insertDebugUtilsLabelEXT(vk::DebugUtilsLabelEXT().setPLabelName(name), dispatcher);
#endif
}

void RendererBackend::initFrameCapture() {
#ifdef ENABLE_RENDERDOC_FRAME_CAPTURE

#ifdef _WIN32
    if (HMODULE mod = GetModuleHandleA("renderdoc.dll")) {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)GetProcAddress(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&g_rdoc.api);
        assert(ret == 1);
    }
#elif __unix__
    if (void* mod = dlopen("librenderdoc.so", RTLD_NOW | RTLD_NOLOAD)) {
        pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void**)&g_rdoc.api);
        assert(ret == 1);
    }
#endif

#endif
}

void RendererBackend::startFrameCapture() {
#ifdef ENABLE_RENDERDOC_FRAME_CAPTURE
    if (!g_rdoc.api) {
        return;
    }

    if (g_rdoc.loop) {
        RENDERDOC_DevicePointer devicePtr = RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(instance->operator VkInstance_T*());
        g_rdoc.api->StartFrameCapture(devicePtr, nullptr);
    } else {
        if (g_rdoc.frameCaptureCount > 0 && g_rdoc.frameDelay == 0) {
            g_rdoc.api->TriggerMultiFrameCapture(g_rdoc.frameCaptureCount);
        }

        --g_rdoc.frameDelay;
    }
#endif
}

void RendererBackend::endFrameCapture() {
#ifdef ENABLE_RENDERDOC_FRAME_CAPTURE
    if (g_rdoc.api && g_rdoc.loop) {
        RENDERDOC_DevicePointer devicePtr = RENDERDOC_DEVICEPOINTER_FROM_VKINSTANCE(instance->operator VkInstance_T*());
        g_rdoc.api->EndFrameCapture(devicePtr, nullptr);
    }
#endif
}

void RendererBackend::setFrameCaptureLoop([[maybe_unused]] bool value) {
#ifdef ENABLE_RENDERDOC_FRAME_CAPTURE
    g_rdoc.loop = value;
#endif
}

void RendererBackend::triggerFrameCapture([[maybe_unused]] uint32_t frameCount, [[maybe_unused]] uint32_t frameDelay) {
#ifdef ENABLE_RENDERDOC_FRAME_CAPTURE
    if (!g_rdoc.api) {
        return;
    }

    g_rdoc.frameCaptureCount = frameCount;
    g_rdoc.frameDelay = frameDelay;
#endif
}

#ifdef ENABLE_VULKAN_VALIDATION

static VKAPI_ATTR VkBool32 VKAPI_CALL vkDebugUtilsCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                           VkDebugUtilsMessageTypeFlagsEXT,
                                                           const VkDebugUtilsMessengerCallbackDataEXT* callbackData,
                                                           void*) {
    EventSeverity mbglSeverity = EventSeverity::Debug;

    switch (messageSeverity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            mbglSeverity = EventSeverity::Debug;
            break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            mbglSeverity = EventSeverity::Info;
            break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            mbglSeverity = EventSeverity::Warning;
            break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            mbglSeverity = EventSeverity::Error;
            break;

        default:
            return VK_FALSE;
    }

    mbgl::Log::Record(mbglSeverity, mbgl::Event::Render, callbackData->pMessage);

    return VK_FALSE;
}

static VKAPI_ATTR VkBool32 vkDebugReportCallback(VkDebugReportFlagsEXT flags,
                                                 VkDebugReportObjectTypeEXT objectType,
                                                 [[maybe_unused]] uint64_t object,
                                                 [[maybe_unused]] size_t location,
                                                 int32_t messageCode,
                                                 const char* pLayerPrefix,
                                                 const char* pMessage,
                                                 [[maybe_unused]] void* pUserData) {
    EventSeverity mbglSeverity = EventSeverity::Debug;

    if (flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) {
        mbglSeverity = EventSeverity::Info;
    } else if (flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
        mbglSeverity = EventSeverity::Warning;
    } else if (flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
        mbglSeverity = EventSeverity::Warning;
    } else if (flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
        mbglSeverity = EventSeverity::Error;
    } else if (flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) {
        mbglSeverity = EventSeverity::Debug;
    }

    const std::string message = "[" + vk::to_string(vk::DebugReportObjectTypeEXT(objectType)) + "]" + "[code - " +
                                std::to_string(messageCode) + "]" + "[layer - " + pLayerPrefix + "]" + pMessage;

    mbgl::Log::Record(mbglSeverity, mbgl::Event::Render, message);

    return VK_FALSE;
}

#endif

void RendererBackend::initDebug() {
#ifdef ENABLE_VULKAN_VALIDATION
    if (debugUtilsEnabled) {
        const vk::DebugUtilsMessageSeverityFlagsEXT severity = vk::DebugUtilsMessageSeverityFlagsEXT() |
                                                               vk::DebugUtilsMessageSeverityFlagBitsEXT::eVerbose |
                                                               vk::DebugUtilsMessageSeverityFlagBitsEXT::eInfo |
                                                               vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
                                                               vk::DebugUtilsMessageSeverityFlagBitsEXT::eError;

        const vk::DebugUtilsMessageTypeFlagsEXT type = vk::DebugUtilsMessageTypeFlagsEXT() |
                                                       vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral |
                                                       vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation |
                                                       vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance;

        const auto createInfo =
            vk::DebugUtilsMessengerCreateInfoEXT().setMessageSeverity(severity).setMessageType(type).setPfnUserCallback(
                vkDebugUtilsCallback);

        debugUtilsCallback = instance->createDebugUtilsMessengerEXTUnique(createInfo, nullptr, dispatcher);

        if (!debugUtilsCallback) {
            mbgl::Log::Error(mbgl::Event::Render, "Failed to register Vulkan debug utils callback");
        }
    } else {
        const vk::DebugReportFlagsEXT flags = vk::DebugReportFlagsEXT() | vk::DebugReportFlagBitsEXT::eDebug |
                                              vk::DebugReportFlagBitsEXT::eError |
                                              vk::DebugReportFlagBitsEXT::eInformation |
                                              vk::DebugReportFlagBitsEXT::ePerformanceWarning |
                                              vk::DebugReportFlagBitsEXT::eWarning;

        const auto createInfo = vk::DebugReportCallbackCreateInfoEXT().setFlags(flags).setPfnCallback(
            vkDebugReportCallback);

        debugReportCallback = instance->createDebugReportCallbackEXTUnique(createInfo, nullptr, dispatcher);

        if (!debugReportCallback) {
            mbgl::Log::Error(mbgl::Event::Render, "Failed to register Vulkan debug report callback");
        }
    }
#endif
}

void RendererBackend::init() {
    // initialize minimal set of function pointers
    dispatcher.init(dynamicLoader);

    initFrameCapture();
    initInstance();

    // initialize function pointers for instance
    dispatcher.init(instance.get());

    initDebug();
    initSurface();
    initDevice();

    // optional function pointer specialization for device
    dispatcher.init(device.get());
    physicalDeviceProperties = physicalDevice.getProperties(dispatcher);
    if (graphicsQueueIndex != -1) graphicsQueue = device->getQueue(graphicsQueueIndex, 0, dispatcher);
    if (presentQueueIndex != -1) presentQueue = device->getQueue(presentQueueIndex, 0, dispatcher);

    initAllocator();
    initSwapchain();
    initCommandPool();
}

void RendererBackend::initInstance() {
    // Vulkan 1.1 on Android is supported on 71% of devices (compared to 1.3 with 6%) as of April 23 2024
    // https://vulkan.gpuinfo.org/
#ifdef __APPLE__
    vk::InstanceCreateFlags instanceFlags = vk::InstanceCreateFlagBits::eEnumeratePortabilityKHR;
#else
    vk::InstanceCreateFlags instanceFlags = {};
#endif

    vk::ApplicationInfo appInfo("maplibre-native", 1, "maplibre-native", 1, VK_API_VERSION_1_0);
    vk::InstanceCreateInfo createInfo(instanceFlags, &appInfo);

    const auto& layers = getLayers();

    bool layersAvailable = checkAvailability(vk::enumerateInstanceLayerProperties(dispatcher),
                                             layers,
                                             [](const vk::LayerProperties& value) { return value.layerName.data(); });

    if (layersAvailable) {
        createInfo.setPEnabledLayerNames(layers);
    } else {
        mbgl::Log::Error(mbgl::Event::Render, "Vulkan layers not found");
    }

    auto extensions = getInstanceExtensions();

    bool extensionsAvailable = checkAvailability(
        vk::enumerateInstanceExtensionProperties(nullptr, dispatcher),
        extensions,
        [](const vk::ExtensionProperties& value) { return value.extensionName.data(); });

#ifdef ENABLE_VULKAN_VALIDATION
    const auto& debugExtensions = getDebugExtensions();
    extensions.insert(extensions.end(), debugExtensions.begin(), debugExtensions.end());

#ifdef ENABLE_VULKAN_GPU_ASSISTED_VALIDATION
    appInfo.setApiVersion(VK_API_VERSION_1_1);

    const std::array<vk::ValidationFeatureEnableEXT, 2> validationFeatures = {
        vk::ValidationFeatureEnableEXT::eGpuAssisted, vk::ValidationFeatureEnableEXT::eGpuAssistedReserveBindingSlot};
    const vk::ValidationFeaturesEXT validationFeatureInfo(validationFeatures);

    createInfo.setPNext(&validationFeatureInfo);
#endif

#endif

    if (extensionsAvailable) {
        createInfo.setPEnabledExtensionNames(extensions);
    } else {
        mbgl::Log::Error(mbgl::Event::Render, "Vulkan extensions not found");
    }

    instance = vk::createInstanceUnique(createInfo, nullptr, dispatcher);
}

void RendererBackend::initSurface() {
    getDefaultRenderable().getResource<SurfaceRenderableResource>().createPlatformSurface();
}

void RendererBackend::initAllocator() {
    VmaVulkanFunctions functions = {};

    functions.vkGetInstanceProcAddr = dispatcher.vkGetInstanceProcAddr;
    functions.vkGetDeviceProcAddr = dispatcher.vkGetDeviceProcAddr;

    functions.vkGetPhysicalDeviceProperties = dispatcher.vkGetPhysicalDeviceProperties;
    functions.vkGetPhysicalDeviceMemoryProperties = dispatcher.vkGetPhysicalDeviceMemoryProperties;
    functions.vkAllocateMemory = dispatcher.vkAllocateMemory;
    functions.vkFreeMemory = dispatcher.vkFreeMemory;
    functions.vkMapMemory = dispatcher.vkMapMemory;
    functions.vkUnmapMemory = dispatcher.vkUnmapMemory;
    functions.vkFlushMappedMemoryRanges = dispatcher.vkFlushMappedMemoryRanges;
    functions.vkInvalidateMappedMemoryRanges = dispatcher.vkInvalidateMappedMemoryRanges;
    functions.vkBindBufferMemory = dispatcher.vkBindBufferMemory;
    functions.vkBindImageMemory = dispatcher.vkBindImageMemory;
    functions.vkGetBufferMemoryRequirements = dispatcher.vkGetBufferMemoryRequirements;
    functions.vkGetImageMemoryRequirements = dispatcher.vkGetImageMemoryRequirements;
    functions.vkCreateBuffer = dispatcher.vkCreateBuffer;
    functions.vkDestroyBuffer = dispatcher.vkDestroyBuffer;
    functions.vkCreateImage = dispatcher.vkCreateImage;
    functions.vkDestroyImage = dispatcher.vkDestroyImage;
    functions.vkCmdCopyBuffer = dispatcher.vkCmdCopyBuffer;
    functions.vkGetBufferMemoryRequirements2KHR = dispatcher.vkGetBufferMemoryRequirements2KHR;
    functions.vkGetImageMemoryRequirements2KHR = dispatcher.vkGetImageMemoryRequirements2KHR;
    functions.vkBindBufferMemory2KHR = dispatcher.vkBindBufferMemory2KHR;
    functions.vkBindImageMemory2KHR = dispatcher.vkBindImageMemory2KHR;
    functions.vkGetPhysicalDeviceMemoryProperties2KHR = dispatcher.vkGetPhysicalDeviceMemoryProperties2KHR;

    VmaAllocatorCreateInfo allocatorCreateInfo = {};

    allocatorCreateInfo.vulkanApiVersion = VK_API_VERSION_1_0;
    allocatorCreateInfo.physicalDevice = physicalDevice;
    allocatorCreateInfo.device = device.get();
    allocatorCreateInfo.instance = instance.get();
    allocatorCreateInfo.pVulkanFunctions = &functions;

    VkResult result = vmaCreateAllocator(&allocatorCreateInfo, &allocator);
    if (result != VK_SUCCESS) {
        throw std::runtime_error("Vulkan allocator init failed");
    }
}

void RendererBackend::initDevice() {
    const auto& extensions = getDeviceExtensions();
    const auto& layers = getLayers();
    const auto& surface = getDefaultRenderable().getResource<SurfaceRenderableResource>().getPlatformSurface().get();

    const auto& isPhysicalDeviceCompatible = [&](const vk::PhysicalDevice& candidate) -> bool {
        bool extensionsAvailable = checkAvailability(
            candidate.enumerateDeviceExtensionProperties(nullptr, dispatcher),
            extensions,
            [](const vk::ExtensionProperties& value) { return value.extensionName.data(); });

        if (!extensionsAvailable) return false;

        graphicsQueueIndex = -1;
        presentQueueIndex = -1;

        const auto& queues = candidate.getQueueFamilyProperties(dispatcher);

        // Use to test on specific GPU type (if multiple)
        // if (candidate.getProperties().deviceType != vk::PhysicalDeviceType::eIntegratedGpu) return false;

        for (auto i = 0u; i < queues.size(); ++i) {
            const auto& queue = queues[i];

            if (queue.queueCount == 0) continue;

            if (queue.queueFlags & vk::QueueFlagBits::eGraphics) graphicsQueueIndex = i;
            if (surface && candidate.getSurfaceSupportKHR(i, surface, dispatcher)) presentQueueIndex = i;

            if (graphicsQueueIndex != -1 && (!surface || presentQueueIndex != -1)) break;
        }

        if (graphicsQueueIndex == -1 || (surface && presentQueueIndex == -1)) return false;

        if (surface) {
            if (candidate.getSurfaceFormatsKHR(surface, dispatcher).empty()) return false;
            if (candidate.getSurfacePresentModesKHR(surface, dispatcher).empty()) return false;
        }

        return true;
    };

    const auto& pickPhysicalDevice = [&]() {
        const auto& physicalDevices = instance->enumeratePhysicalDevices(dispatcher);
        if (physicalDevices.empty()) throw std::runtime_error("No Vulkan compatible GPU found");

        for (const auto& candidate : physicalDevices) {
            if (isPhysicalDeviceCompatible(candidate)) {
                physicalDevice = candidate;
                break;
            }
        }

        if (!physicalDevice) throw std::runtime_error("No suitable GPU found");
    };

    pickPhysicalDevice();

    std::vector<vk::DeviceQueueCreateInfo> queueCreateInfos;

    float queuePriority = 1.0f;

    queueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags(), graphicsQueueIndex, 1, &queuePriority);

    if (surface && graphicsQueueIndex != presentQueueIndex)
        queueCreateInfos.emplace_back(vk::DeviceQueueCreateFlags(), presentQueueIndex, 1, &queuePriority);

    [[maybe_unused]] const auto& supportedDeviceFeatures = physicalDevice.getFeatures(dispatcher);
    physicalDeviceFeatures = vk::PhysicalDeviceFeatures();

    // TODO
    // - WideLines disabled on Android (20.77% device coverage https://vulkan.gpuinfo.org/listfeaturescore10.php)
    // - Rework this to a dynamic toggle based on MLN_TRIANGULATE_FILL_OUTLINES/MLN_ENABLE_POLYLINE_DRAWABLES
#if !defined(__ANDROID__) && !defined(__APPLE__)
    if (supportedDeviceFeatures.wideLines) {
        physicalDeviceFeatures.setWideLines(true);

        // more wideLines info
        // physicalDeviceProperties.limits.lineWidthRange;
        // physicalDeviceProperties.limits.lineWidthGranularity;
    } else {
        mbgl::Log::Error(mbgl::Event::Render, "Feature not available: wideLines");
    }
#endif

    if (supportedDeviceFeatures.samplerAnisotropy) {
        physicalDeviceFeatures.setSamplerAnisotropy(true);
    } else {
        mbgl::Log::Error(mbgl::Event::Render, "Feature not available: samplerAnisotropy");
    }

    auto createInfo = vk::DeviceCreateInfo()
                          .setQueueCreateInfos(queueCreateInfos)
                          .setPEnabledExtensionNames(extensions)
                          .setPEnabledFeatures(&physicalDeviceFeatures);

    // this is not needed for newer implementations
    createInfo.setPEnabledLayerNames(layers);

    device = physicalDevice.createDeviceUnique(createInfo, nullptr, dispatcher);
}

void RendererBackend::initSwapchain() {
    auto& renderable = getDefaultRenderable();
    auto& renderableResource = renderable.getResource<SurfaceRenderableResource>();
    const auto& size = renderable.getSize();

    // buffer resources if rendering to a surface
    // no buffering when using headless
    maxFrames = renderableResource.getPlatformSurface() ? 2 : 1;

    renderableResource.init(size.width, size.height);

    if (renderableResource.hasSurfaceTransformSupport()) {
        auto& renderableImpl = static_cast<Renderable&>(renderable);
        const auto& extent = renderableResource.getExtent();

        renderableImpl.setSize({extent.width, extent.height});
    }
}

void RendererBackend::initCommandPool() {
    const vk::CommandPoolCreateInfo createInfo(vk::CommandPoolCreateFlagBits::eResetCommandBuffer, graphicsQueueIndex);
    commandPool = device->createCommandPoolUnique(createInfo, nullptr, dispatcher);
}

void RendererBackend::destroyResources() {
    if (device) device->waitIdle(dispatcher);

    context.reset();
    commandPool.reset();

    vmaDestroyAllocator(allocator);

    usingSharedContext ? void(device.release()) : device.reset();

    // destroy this last so we have cleanup validation
    debugUtilsCallback.reset();
    debugReportCallback.reset();

    usingSharedContext ? void(instance.release()) : instance.reset();
}

/// @brief Register a list of types with a shader registry instance
/// @tparam ...ShaderID Pack of BuiltIn:: shader IDs
/// @param registry A shader registry instance
/// @param programParameters ProgramParameters used to initialize each instance
template <shaders::BuiltIn... ShaderID>
void registerTypes(gfx::ShaderRegistry& registry, const ProgramParameters& programParameters) {
    /// The following fold expression will create a shader for every type
    /// in the parameter pack and register it with the shader registry.

    // Registration calls are wrapped in a lambda that throws on registration
    // failure, we shouldn't expect registration to faill unless the shader
    // registry instance provided already has conflicting programs present.
    (
        [&]() {
            using namespace std::string_literals;
            using ShaderClass = shaders::ShaderSource<ShaderID, gfx::Backend::Type::Vulkan>;
            auto group = std::make_shared<ShaderGroup<ShaderID>>(programParameters);
            if (!registry.registerShaderGroup(std::move(group), ShaderClass::name)) {
                assert(!"duplicate shader group");
                throw std::runtime_error("Failed to register "s + ShaderClass::name + " with shader registry!");
            }
        }(),
        ...);
}

void RendererBackend::initShaders(gfx::ShaderRegistry& shaders, const ProgramParameters& programParameters) {
    registerTypes<shaders::BuiltIn::BackgroundShader,
                  shaders::BuiltIn::BackgroundPatternShader,
                  shaders::BuiltIn::CircleShader,
                  shaders::BuiltIn::ClippingMaskProgram,
                  shaders::BuiltIn::CollisionBoxShader,
                  shaders::BuiltIn::CollisionCircleShader,
                  shaders::BuiltIn::CustomGeometryShader,
                  shaders::BuiltIn::CustomSymbolIconShader,
                  shaders::BuiltIn::DebugShader,
                  shaders::BuiltIn::FillShader,
                  shaders::BuiltIn::FillOutlineShader,
                  shaders::BuiltIn::FillPatternShader,
                  shaders::BuiltIn::FillOutlinePatternShader,
                  shaders::BuiltIn::FillOutlineTriangulatedShader,
                  shaders::BuiltIn::FillExtrusionShader,
                  shaders::BuiltIn::FillExtrusionPatternShader,
                  shaders::BuiltIn::HeatmapShader,
                  shaders::BuiltIn::HeatmapTextureShader,
                  shaders::BuiltIn::HillshadeShader,
                  shaders::BuiltIn::HillshadePrepareShader,
                  shaders::BuiltIn::LineShader,
                  shaders::BuiltIn::LineGradientShader,
                  shaders::BuiltIn::LineSDFShader,
                  shaders::BuiltIn::LinePatternShader,
                  shaders::BuiltIn::LocationIndicatorShader,
                  shaders::BuiltIn::LocationIndicatorTexturedShader,
                  shaders::BuiltIn::RasterShader,
                  shaders::BuiltIn::SymbolIconShader,
                  shaders::BuiltIn::SymbolSDFShader,
                  shaders::BuiltIn::SymbolTextAndIconShader,
                  shaders::BuiltIn::WideVectorShader>(shaders, programParameters);
}

} // namespace vulkan
} // namespace mbgl
