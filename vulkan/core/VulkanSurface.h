#ifndef VULKAN_SURFACE_H
#define VULKAN_SURFACE_H

#include "VRVulkan.h"
#include "VulkanWrapper.h"
#include "NonCopyable.h"

namespace VR {
namespace backend {

class VulkanSurface : public NonCopyable {
public:    
    void* createVkSurfaceKHR(void* nativeWindow, void* vkinstance, uint64_t flags); 
    VulkanSurface() = default;
    virtual ~VulkanSurface(){};
};   

} // namespace backend
} // namespace VR

#endif // VULKAN_SURFACE_H
