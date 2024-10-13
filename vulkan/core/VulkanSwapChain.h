#ifndef VULKAN_SWAPCHAIN_H
#define VULKAN_SWAPCHAIN_H

#include "VulkanContext.h"

namespace VR {
namespace backend {

class VulkanSwapChain : public NonCopyable {
public:    
    VulkanSwapChain(VulkanContext& context, VkSurfaceKHR vksurface);
    VulkanSwapChain(VulkanContext& context, uint32_t width, uint32_t height);

    bool acquireNextImage();
    void create();
    void destroy();
    void makePresentable();
    bool hasResized() const;
    
    void getHeadlessQueue();
    void getPresentQueue();
    
public:
    VulkanContext& context;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkSurfaceFormatKHR surfaceFormat;
    VkExtent2D clientSize;
    VkQueue presentQueue;
    VkQueue headlessQueue;
    std::vector<VulkanAttachment> colorAttachments;
    VulkanAttachment depthAttachment;
    uint32_t currentSwapIndex;
    VkSemaphore nextImageAvailable;

    bool acquired;
    bool suboptimal;
};

} // namespace backend
} // namespace VR

#endif // VULKAN_SWAPCHAIN_H
