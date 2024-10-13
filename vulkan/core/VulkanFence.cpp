#include "VulkanFence.h"
#include "VulkanAlloc.h"

namespace VR {
namespace backend {

VulkanFence::VulkanFence(const VkDevice& device, bool signaled) : mDevice(device) {

    VkFenceCreateInfo fenceCreateInfo { .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO };
    if (signaled) {
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }
    vkCreateFence(mDevice, &fenceCreateInfo, VKALLOC, &mFence);
}
VulkanFence::~VulkanFence() {
    vkDestroyFence(mDevice, mFence, VKALLOC);
}

VkResult VulkanFence::wait(uint64_t timeout) const {
    auto status = VK_TIMEOUT;
    do {
        status = vkWaitForFences(mDevice, 1, &mFence, VK_TRUE, timeout);
    } while (status == VK_TIMEOUT);
    return status;
}

VkResult VulkanFence::reset() const {
    return vkResetFences(mDevice, 1, &mFence);
}

} // namespace backend
} // namespace VR
