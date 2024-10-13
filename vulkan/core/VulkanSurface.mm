#include "VulkanSurface.h"
#include "VulkanAlloc.h"

#include <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

namespace VR {
namespace backend {

void* VulkanSurface::createVkSurfaceKHR(void* nativeWindow, void* instance, uint64_t flags) {

    NSView* nsview = (__bridge NSView*) nativeWindow;
    VR_VK_ASSERT(nsview != nullptr, "Unable to get NSView");

    // create the VkSurface
    VR_VK_ASSERT(vkCreateMacOSSurfaceMVK != nullptr, "Unable to load vkCreateMacOSSurfaceMVK");
    VkSurfaceKHR surface = nullptr;
    VkMacOSSurfaceCreateInfoMVK createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_MACOS_SURFACE_CREATE_INFO_MVK;
    createInfo.pView = (__bridge void*) nsview;
    VkResult result = vkCreateMacOSSurfaceMVK((VkInstance) instance, &createInfo, VKALLOC, &surface);
    VR_VK_ASSERT(result == VK_SUCCESS, "vkCreateMacOSSurfaceMVK error");

    return surface;
}

} // namespace backend
} // namespace VR
