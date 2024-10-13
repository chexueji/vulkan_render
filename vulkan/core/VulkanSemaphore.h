#ifndef VULKAN_SEMAPHORE_H
#define VULKAN_SEMAPHORE_H

#include "NonCopyable.h"
#include "VulkanWrapper.h"

namespace VR {
namespace backend { 

class VulkanSemaphore : public NonCopyable {
public:
    VulkanSemaphore(const VkDevice& device);
    virtual ~VulkanSemaphore();

    VkSemaphore get() const {
        return mSemaphore;
    }

private:
    VkSemaphore mSemaphore;
    const VkDevice& mDevice;
};

} // backend
} // VR
#endif // VULKAN_SEMAPHORE_H
