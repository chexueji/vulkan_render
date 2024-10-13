#include "VulkanUtils.h"
#include "VulkanContext.h"
#include "VulkanTexture.h"
#include "VulkanMemoryPool.h"
#include "VulkanCommandPool.h"
#include "VulkanSwapChain.h"

#ifndef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
#define VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME "VK_KHR_portability_subset"
#endif

namespace VR {
namespace backend {

void selectPhysicalDevice(VulkanContext& context) {
    uint32_t physicalDeviceCount = 0;
    VkResult result = vkEnumeratePhysicalDevices(context.instance, &physicalDeviceCount, nullptr);
    VR_VK_CHECK(result == VK_SUCCESS && physicalDeviceCount > 0, "vkEnumeratePhysicalDevices count error.");

    std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
    result = vkEnumeratePhysicalDevices(context.instance, &physicalDeviceCount, physicalDevices.data());
    VR_VK_CHECK(result == VK_SUCCESS, "vkEnumeratePhysicalDevices error.");

    context.physicalDevice = nullptr;
    for (uint32_t i = 0; i < physicalDeviceCount; ++i) {
        VkPhysicalDevice physicalDevice = physicalDevices[i];
        vkGetPhysicalDeviceProperties(physicalDevice, &context.physicalDeviceProperties);

        const int major = VK_VERSION_MAJOR(context.physicalDeviceProperties.apiVersion);
        const int minor = VK_VERSION_MINOR(context.physicalDeviceProperties.apiVersion);
        VR_PRINT("Vulkan api version %d_%d\n", major, minor);

        if (major < VK_REQUIRED_VERSION_MAJOR) {
            continue;
        }
        if (major == VK_REQUIRED_VERSION_MAJOR && minor < VK_REQUIRED_VERSION_MINOR) {
            continue;
        }

        uint32_t queueFamiliesCount;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, nullptr);
        if (queueFamiliesCount == 0) {
            continue;
        }
        std::vector<VkQueueFamilyProperties> queueFamiliesProperties(queueFamiliesCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamiliesCount, queueFamiliesProperties.data());
        context.graphicsQueueFamilyIndex = 0xffff;
        for (uint32_t j = 0; j < queueFamiliesCount; ++j) {
            VkQueueFamilyProperties props = queueFamiliesProperties[j];
            if (props.queueCount == 0) {
                continue;
            }
#if defined(DEBUG) && defined(VR_SHARED_GPU_MEMORY_VULKAN)
            VR_PRINT("Queue Num:%d\n", props.queueCount);
            VR_PRINT("Queue Flags:%s\n", std::to_string(props.queueFlags).c_str());
#endif
#if defined(VR_SHARED_GPU_MEMORY_VULKAN)
            if (props.queueFlags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)) {
                context.graphicsQueueFamilyIndex = j;
            }
#else
            if (props.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                context.graphicsQueueFamilyIndex = j;
            }
#endif
        }
        if (context.graphicsQueueFamilyIndex == 0xffff) continue;

        // support the VK_KHR_swapchain
        uint32_t extensionCount;
        result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);
        VR_VK_CHECK(result == VK_SUCCESS, "vkEnumerateDeviceExtensionProperties count error.");

        std::vector<VkExtensionProperties> extensions(extensionCount);
        result = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, extensions.data());
        VR_VK_CHECK(result == VK_SUCCESS, "vkEnumerateDeviceExtensionProperties error.");
        
        bool supportsSwapchain = false;
        context.debugMarkersSupported = false;
        for (uint32_t k = 0; k < extensionCount; ++k) {
            if (!strcmp(extensions[k].extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
                supportsSwapchain = true;
            }
            if (!strcmp(extensions[k].extensionName, VK_EXT_DEBUG_MARKER_EXTENSION_NAME)) {
                context.debugMarkersSupported = true;
            }
            if (!strcmp(extensions[k].extensionName, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) {
                context.portabilitySubsetSupported = true;
            }
            if (!strcmp(extensions[k].extensionName, VK_KHR_MAINTENANCE1_EXTENSION_NAME)) {
                context.maintenanceSupported[0] = true;
            }
            if (!strcmp(extensions[k].extensionName, VK_KHR_MAINTENANCE2_EXTENSION_NAME)) {
                context.maintenanceSupported[1] = true;
            }
            if (!strcmp(extensions[k].extensionName, VK_KHR_MAINTENANCE3_EXTENSION_NAME)) {
                context.maintenanceSupported[2] = true;
            }
        }
        if (!supportsSwapchain) continue;

        context.physicalDevice = physicalDevice;
        vkGetPhysicalDeviceFeatures(physicalDevice, &context.physicalDeviceFeatures);
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &context.memoryProperties);

        if (vkGetPhysicalDeviceProperties2) {
            VkPhysicalDeviceDriverProperties driverProperties = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES,
            };
            VkPhysicalDeviceProperties2 physicalDeviceProperties2 = {
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
                .pNext = &driverProperties,
            };
            vkGetPhysicalDeviceProperties2(physicalDevice, &physicalDeviceProperties2);
            VR_PRINT("Vulkan device driver: %s %s", driverProperties.driverName, driverProperties.driverInfo);
        }

        return;
    }
    VR_ERROR("Unable to find suitable device.");
}

void createLogicalDevice(VulkanContext& context) {
    VkDeviceQueueCreateInfo deviceQueueCreateInfo[1] = {};
    const float queuePriority[] = {1.0f};
    VkDeviceCreateInfo deviceCreateInfo = {};
    std::vector<const char*> deviceExtensionNames = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    if (context.debugMarkersSupported && !context.debugUtilsSupported) {
        deviceExtensionNames.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
    }
    if (context.portabilitySubsetSupported) {
        deviceExtensionNames.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
    }
    if (context.maintenanceSupported[0]) {
        deviceExtensionNames.push_back(VK_KHR_MAINTENANCE1_EXTENSION_NAME);
    }
    if (context.maintenanceSupported[1]) {
        deviceExtensionNames.push_back(VK_KHR_MAINTENANCE2_EXTENSION_NAME);
    }
    if (context.maintenanceSupported[2]) {
        deviceExtensionNames.push_back(VK_KHR_MAINTENANCE3_EXTENSION_NAME);
    }

    deviceQueueCreateInfo->sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    deviceQueueCreateInfo->queueFamilyIndex = context.graphicsQueueFamilyIndex;
    deviceQueueCreateInfo->queueCount = 1;
    deviceQueueCreateInfo->pQueuePriorities = &queuePriority[0];
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = deviceQueueCreateInfo;

    const auto& supportedFeatures = context.physicalDeviceFeatures;
    VkPhysicalDeviceFeatures enabledFeatures {
        .samplerAnisotropy = supportedFeatures.samplerAnisotropy,
        .textureCompressionETC2 = supportedFeatures.textureCompressionETC2,
        .textureCompressionBC = supportedFeatures.textureCompressionBC,
    };

    deviceCreateInfo.pEnabledFeatures = &enabledFeatures;
    deviceCreateInfo.enabledExtensionCount = (uint32_t)deviceExtensionNames.size();
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensionNames.data();

#if defined(VK_ENABLE_BETA_EXTENSIONS)
    VkPhysicalDevicePortabilitySubsetFeaturesKHR portability = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PORTABILITY_SUBSET_FEATURES_KHR,
        .pNext = nullptr,
        .mutableComparisonSamplers = VK_TRUE,
    };
    if (context.portabilitySubsetSupported) {
        deviceCreateInfo.pNext = &portability;
    }
#endif // VK_ENABLE_BETA_EXTENSIONS

    VkResult result = vkCreateDevice(context.physicalDevice, &deviceCreateInfo, VKALLOC,
            &context.device);
    VR_VK_CHECK(result == VK_SUCCESS, "vkCreateDevice error.");
    vkGetDeviceQueue(context.device, context.graphicsQueueFamilyIndex, 0,
            &context.graphicsQueue);

    VkCommandPoolCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    createInfo.queueFamilyIndex = context.graphicsQueueFamilyIndex;
    result = vkCreateCommandPool(context.device, &createInfo, VKALLOC, &context.commandPool);
    VR_VK_CHECK(result == VK_SUCCESS, "vkCreateCommandPool error.");

    const VmaVulkanFunctions funcs {
        .vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties,
        .vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties,
        .vkAllocateMemory = vkAllocateMemory,
        .vkFreeMemory = vkFreeMemory,
        .vkMapMemory = vkMapMemory,
        .vkUnmapMemory = vkUnmapMemory,
        .vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges,
        .vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges,
        .vkBindBufferMemory = vkBindBufferMemory,
        .vkBindImageMemory = vkBindImageMemory,
        .vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements,
        .vkGetImageMemoryRequirements = vkGetImageMemoryRequirements,
        .vkCreateBuffer = vkCreateBuffer,
        .vkDestroyBuffer = vkDestroyBuffer,
        .vkCreateImage = vkCreateImage,
        .vkDestroyImage = vkDestroyImage,
        .vkCmdCopyBuffer = vkCmdCopyBuffer,
        .vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2,
        .vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2
    };
    const VmaAllocatorCreateInfo allocatorInfo {
        .physicalDevice = context.physicalDevice,
        .device = context.device,
        .pVulkanFunctions = &funcs,
        .pRecordSettings = nullptr,
        .instance = context.instance
    };

    vmaCreateAllocator(&allocatorInfo, &context.allocator);
}


uint32_t selectMemoryType(VulkanContext& context, uint32_t flags, VkFlags reqs) {
    for (uint32_t i = 0; i < VK_MAX_MEMORY_TYPES; i++) {
        if (flags & 1) {
            if ((context.memoryProperties.memoryTypes[i].propertyFlags & reqs) == reqs) {
                return i;
            }
        }
        flags >>= 1;
    }
    VR_VK_CHECK(false, "Unable to find a memory type that meets requirements.");
    return (uint32_t) ~0ul;
}

VulkanAttachment& getSwapChainColorAttachment(VulkanContext& context) {
    VulkanSwapChain& surface = *context.currentSwapChain;
    return surface.colorAttachments[surface.currentSwapIndex];
}

VulkanAttachment& getSwapChainDepthAttachment(VulkanContext& context)
{
    VulkanSwapChain& surface = *context.currentSwapChain;
    return surface.depthAttachment;
}

uint32_t getSwapChainIndex(VulkanContext& context) {
    VulkanSwapChain& surface = *context.currentSwapChain;
    return surface.currentSwapIndex;
}

void waitForIdle(VulkanContext& context) {

    if (!context.device) {
        return;
    }
    context.commandpool->flush();
    context.commandpool->wait();
}

VkFormat findSupportedFormat(VulkanContext& context, const std::vector<VkFormat>& candidates,
        VkImageTiling tiling, VkFormatFeatureFlags features) {
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(context.physicalDevice, format, &props);
        if (tiling == VK_IMAGE_TILING_LINEAR &&
                (props.linearTilingFeatures & features) == features) {
            return format;
        } else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
                (props.optimalTilingFeatures & features) == features) {
            return format;
        }
    }
    return VK_FORMAT_UNDEFINED;
}

VkImageLayout getTextureLayout(TextureUsage usage) {

    // use GENERAL for depth textures
    if (any(usage & TextureUsage::DEPTH_ATTACHMENT)) {
        return VK_IMAGE_LAYOUT_GENERAL;
    }

    // use GENERAL for color attachable textures
    if (any(usage & TextureUsage::COLOR_ATTACHMENT)) {
        return VK_IMAGE_LAYOUT_GENERAL;
    }

    // for immutable textures: read-only.
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

} // namespace backend
} // namespace VR
