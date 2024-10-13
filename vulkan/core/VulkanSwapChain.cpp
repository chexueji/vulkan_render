#include "VulkanCommandPool.h"
#include "VulkanSwapChain.h"

namespace VR {
namespace backend {

bool VulkanSwapChain::acquireNextImage() {
    if (headlessQueue) {
        currentSwapIndex = (currentSwapIndex + 1) % colorAttachments.size();
        return true;
    }
    // acquire next drawbale image
    VkResult result = vkAcquireNextImageKHR(context.device, swapchain, UINT64_MAX, nextImageAvailable, VK_NULL_HANDLE, &currentSwapIndex);

    if (result == VK_SUBOPTIMAL_KHR && !suboptimal) {
        VR_PRINT("Vulkan: Suboptimal swap chain.");
        suboptimal = true;
    }

    context.commandpool->setAcquireNextImageSignal(nextImageAvailable);
    acquired = true;

    VR_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);
    return true;
}

static void createDepthImage(VulkanContext& context, VulkanSwapChain& surfaceContext, VkFormat depthFormat, VkExtent2D size) {
    
    VkImage depthImage;
    VkImageCreateInfo imageInfo {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = depthFormat,
        .extent = { size.width, size.height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    };

    VkResult result = vkCreateImage(context.device, &imageInfo, VKALLOC, &depthImage);
    VR_VK_CHECK(result == VK_SUCCESS, "Unable to create depth image.");

    // bind image
    VkMemoryRequirements memReqs;
    vkGetImageMemoryRequirements(context.device, depthImage, &memReqs);
    VkMemoryAllocateInfo allocInfo {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = selectMemoryType(context, memReqs.memoryTypeBits,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };

    result = vkAllocateMemory(context.device, &allocInfo, nullptr, &surfaceContext.depthAttachment.memory);
    VR_VK_CHECK(result == VK_SUCCESS, "Unable to allocate depth memory.");

    result = vkBindImageMemory(context.device, depthImage, surfaceContext.depthAttachment.memory, 0);
    VR_VK_CHECK(result == VK_SUCCESS, "Unable to bind depth memory.");

    // attach depth to the framebuffer
    VkImageView depthView;
    VkImageViewCreateInfo viewInfo {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = depthImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = depthFormat,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    result = vkCreateImageView(context.device, &viewInfo, VKALLOC, &depthView);
    VR_VK_CHECK(result == VK_SUCCESS, "Unable to create depth view.");

    // only need depth attachment
    surfaceContext.depthAttachment.view = depthView;
    surfaceContext.depthAttachment.image = depthImage;
    surfaceContext.depthAttachment.format = depthFormat;
    surfaceContext.depthAttachment.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // transitioning the image layout
    VkCommandBuffer cmdbuffer = context.commandpool->get().cmdbuffer;

    // transition the depth image layout
    VkImageMemoryBarrier barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .newLayout = surfaceContext.depthAttachment.layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = depthImage,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };
    vkCmdPipelineBarrier(cmdbuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

void VulkanSwapChain::create() {

    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.physicalDevice, surface, &caps);

    // MAILBOX
    const uint32_t maxImageCount = caps.maxImageCount;
    const uint32_t minImageCount = caps.minImageCount;
    uint32_t desiredImageCount = minImageCount + 1;

    if (maxImageCount != 0 && desiredImageCount > maxImageCount) {
        VR_ERROR("Swap chain does not support %d images.", desiredImageCount);
        desiredImageCount = caps.minImageCount;
    }
    // FIXME:should uncomment
    // if (surfaceFormat.format == VK_FORMAT_UNDEFINED) {

        uint32_t surfaceFormatsCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(context.physicalDevice, surface, &surfaceFormatsCount, nullptr);

        std::vector<VkSurfaceFormatKHR> surfaceFormats(surfaceFormatsCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(context.physicalDevice, surface, &surfaceFormatsCount, surfaceFormats.data());

        surfaceFormat = surfaceFormats[0];
        for (const VkSurfaceFormatKHR& format : surfaceFormats) {
            if (format.format == VK_FORMAT_R8G8B8A8_UNORM) {
                surfaceFormat = format;
            }
        }
    // }

    clientSize = caps.currentExtent;

    const auto compositionCaps = caps.supportedCompositeAlpha;
    const auto compositeAlpha = (compositionCaps & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) ? VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR : VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

    // create swap chain
    VkSwapchainCreateInfoKHR createInfo {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = surface,
        .minImageCount = desiredImageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = clientSize,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, 
        .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .compositeAlpha = compositeAlpha,
        .presentMode = VK_PRESENT_MODE_FIFO_KHR,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE
    };

    VkResult result = vkCreateSwapchainKHR(context.device, &createInfo, VKALLOC, &swapchain);
    VR_VK_ASSERT(result == VK_SUCCESS, "vkCreateSwapchainKHR error.");

    // get image of swap chain
    uint32_t imageCount;
    result = vkGetSwapchainImagesKHR(context.device, swapchain, &imageCount, nullptr);
    VR_VK_ASSERT(result == VK_SUCCESS, "vkGetSwapchainImagesKHR count error.");

    colorAttachments.resize(imageCount);
    std::vector<VkImage> images(imageCount);
    result = vkGetSwapchainImagesKHR(context.device, swapchain, &imageCount, images.data());
    VR_VK_ASSERT(result == VK_SUCCESS, "vkGetSwapchainImagesKHR error.");

    for (size_t i = 0; i < images.size(); ++i) {
        VulkanAttachment attachment;
        attachment.format = surfaceFormat.format;
        attachment.image = images[i];
        attachment.view = {};
        attachment.memory = {};
        attachment.texture = {};
        attachment.layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        attachment.baseMipLevel = 0u;
        attachment.baseLayer = 0u;
        
        colorAttachments[i] = attachment;
    }

    // create image view which we can render into
    // each swapchain image has its own command buffer and othre resources for easily using and tracking
    VkImageViewCreateInfo ivCreateInfo = {};
    ivCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ivCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ivCreateInfo.format = surfaceFormat.format;
    ivCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ivCreateInfo.subresourceRange.levelCount = 1;
    ivCreateInfo.subresourceRange.layerCount = 1;
    ivCreateInfo.components.r                = VK_COMPONENT_SWIZZLE_R;
    ivCreateInfo.components.g                = VK_COMPONENT_SWIZZLE_G;
    ivCreateInfo.components.b                = VK_COMPONENT_SWIZZLE_B;
    ivCreateInfo.components.a                = VK_COMPONENT_SWIZZLE_A;
    for (size_t i = 0; i < images.size(); ++i) {
        ivCreateInfo.image = images[i];
        result = vkCreateImageView(context.device, &ivCreateInfo, VKALLOC, &colorAttachments[i].view);
        VR_VK_ASSERT(result == VK_SUCCESS, "vkCreateImageView error.");
    }

    createSemaphore(context.device, &nextImageAvailable);
    acquired = false;

    createDepthImage(context, *this, context.depthFormat, clientSize);
}

void VulkanSwapChain::destroy() {
    waitForIdle(context);
    const VkDevice device = context.device;
    for (VulkanAttachment& swapContext : colorAttachments) {

        if (!swapchain) {
            vkDestroyImage(device, swapContext.image, VKALLOC);
            vkFreeMemory(device, swapContext.memory, VKALLOC);
        }

        vkDestroyImageView(device, swapContext.view, VKALLOC);
        swapContext.view = VK_NULL_HANDLE;
    }

    vkDestroySwapchainKHR(device, swapchain, VKALLOC);
    vkDestroySemaphore(device, nextImageAvailable, VKALLOC);

    vkDestroyImageView(device, depthAttachment.view, VKALLOC);
    vkDestroyImage(device, depthAttachment.image, VKALLOC);
    vkFreeMemory(device, depthAttachment.memory, VKALLOC);
}

void VulkanSwapChain::makePresentable() {
    if (headlessQueue) {
        return;
    }

    VulkanAttachment& swapContext = colorAttachments[currentSwapIndex];
    VkImageMemoryBarrier barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = 0,
#ifdef ANDROID
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
#else
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
#endif
        .newLayout = swapContext.layout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = swapContext.image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .levelCount = 1,
            .layerCount = 1,
        },
    };

    vkCmdPipelineBarrier(context.commandpool->get().cmdbuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

// offscreen rendering
void VulkanSwapChain::getHeadlessQueue() {
    vkGetDeviceQueue(context.device, context.graphicsQueueFamilyIndex, 0, &headlessQueue);
    VR_VK_ASSERT(headlessQueue != VK_NULL_HANDLE, "Unable to obtain graphics queue.");
    // reset present queue
    presentQueue = VK_NULL_HANDLE;
}

void VulkanSwapChain::getPresentQueue() {

    uint32_t queueFamiliesCount;
    vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice, &queueFamiliesCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamiliesProperties(queueFamiliesCount);
    vkGetPhysicalDeviceQueueFamilyProperties(context.physicalDevice, &queueFamiliesCount,
            queueFamiliesProperties.data());
    uint32_t presentQueueFamilyIndex = 0xffff;

    VkBool32 supported = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(context.physicalDevice, context.graphicsQueueFamilyIndex, surface, &supported);
    if (supported) {
        presentQueueFamilyIndex = context.graphicsQueueFamilyIndex;
    }

    // fall back to separate graphics and present queues
    if (presentQueueFamilyIndex == 0xffff) {
        for (uint32_t j = 0; j < queueFamiliesCount; ++j) {
            vkGetPhysicalDeviceSurfaceSupportKHR(context.physicalDevice, j, surface, &supported);
            if (supported) {
                presentQueueFamilyIndex = j;
                break;
            }
        }
    }

    VR_VK_ASSERT(presentQueueFamilyIndex != 0xffff, "Does not support the presentation queue.");

    if (context.graphicsQueueFamilyIndex != presentQueueFamilyIndex) {
        vkGetDeviceQueue(context.device, presentQueueFamilyIndex, 0, &presentQueue);

    } else {
        presentQueue = context.graphicsQueue;
    }

    VR_VK_ASSERT(presentQueue != VK_NULL_HANDLE, "Unable to obtain presentation queue.");
    headlessQueue = VK_NULL_HANDLE;
}

VulkanSwapChain::VulkanSwapChain(VulkanContext& context, VkSurfaceKHR vksurface) : context(context) {
    suboptimal = false;
    surface = vksurface;
    getPresentQueue();
    create();
}

// for offscreen render
VulkanSwapChain::VulkanSwapChain(VulkanContext& context, uint32_t width, uint32_t height) : context(context) {
    
    surface = VK_NULL_HANDLE;
    getHeadlessQueue();

    surfaceFormat.format = VK_FORMAT_B8G8R8A8_UNORM; // FIXME: should be VK_FORMAT_R8G8B8A8_UNORM
    swapchain = VK_NULL_HANDLE;

    // double buffered for headless rendering
    colorAttachments.resize(2);

    for (size_t i = 0; i < colorAttachments.size(); ++i) {
        VkImage image;

        VkImageCreateInfo iCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = surfaceFormat.format,
            .extent = {
                .width = width,
                .height = height,
                .depth = 1,
            },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        };

        VR_ASSERT(iCreateInfo.extent.width > 0);
        VR_ASSERT(iCreateInfo.extent.height > 0);

        vkCreateImage(context.device, &iCreateInfo, VKALLOC, &image);

        VkMemoryRequirements memReqs = {};
        vkGetImageMemoryRequirements(context.device, image, &memReqs);
        VkMemoryAllocateInfo allocInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .allocationSize = memReqs.size,
            .memoryTypeIndex = selectMemoryType(context, memReqs.memoryTypeBits,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
        };

        VkDeviceMemory imageMemory;
        vkAllocateMemory(context.device, &allocInfo, VKALLOC, &imageMemory);
        vkBindImageMemory(context.device, image, imageMemory, 0);

        VulkanAttachment attachment;
        attachment.format = surfaceFormat.format;
        attachment.image = image;
        attachment.view = {};
        attachment.memory = imageMemory;
        attachment.texture = nullptr;
        attachment.layout = VK_IMAGE_LAYOUT_GENERAL;
        attachment.baseMipLevel = 0u;
        attachment.baseLayer = 0u;
        
        colorAttachments[i] = attachment;

        VkImageViewCreateInfo ivCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = image,
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = surfaceFormat.format,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            }
        };

        vkCreateImageView(context.device, &ivCreateInfo, VKALLOC, &colorAttachments[i].view);

        VkImageMemoryBarrier barrier {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = image,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .levelCount = 1,
                .layerCount = 1,
            },
        };

        vkCmdPipelineBarrier(context.commandpool->get().cmdbuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0, nullptr, 1,
                &barrier);
    }

    clientSize.width = width;
    clientSize.height = height;

    nextImageAvailable = VK_NULL_HANDLE;

    createDepthImage(context, *this, context.depthFormat, clientSize);
}

bool VulkanSwapChain::hasResized() const {
    if (surface == VK_NULL_HANDLE) {
        return false;
    }

    VkSurfaceCapabilitiesKHR surfaceCapabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.physicalDevice, surface, &surfaceCapabilities);
    return !equivalent(clientSize, surfaceCapabilities.currentExtent);
}

} // namespace backend
} // namespace VR
