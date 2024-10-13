
#ifndef VULKAN_FENCE_H
#define VULKAN_FENCE_H

#include "NonCopyable.h"
#include "VulkanWrapper.h"

namespace VR {
namespace backend {

class VulkanFence : public NonCopyable {
public:
    VulkanFence(const VkDevice& device, bool signaled = false);
    virtual ~VulkanFence();

    VkFence get() const {
        return mFence;
    }
    VkResult reset() const;
    VkResult wait(uint64_t timeout) const;

private:
    VkFence mFence;
    const VkDevice& mDevice;
};

} // namespace backend
} // namespace VR

#endif // VULKAN_FENCE_H
