#include "VulkanSemaphore.h"
#include "VulkanAlloc.h"

namespace VR {
namespace backend {

VulkanSemaphore::VulkanSemaphore(const VkDevice& device) : mDevice(device) {

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType                 = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    semaphoreInfo.flags                 = 0;
    semaphoreInfo.pNext                 = nullptr;
    vkCreateSemaphore(mDevice, &semaphoreInfo, VKALLOC, &mSemaphore);
}
VulkanSemaphore::~VulkanSemaphore() {
    vkDestroySemaphore(mDevice, mSemaphore, VKALLOC);
}

} // namespace backend
} // namespace VR
