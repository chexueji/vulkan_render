#include "VulkanMacros.h"
#include "VulkanCommandPool.h"
#include "VulkanFence.h"
#include "VulkanAlloc.h"

namespace VR {
namespace backend {

CommandBufferObserver::~CommandBufferObserver() {}

VulkanCommandPool::VulkanCommandPool(const VkDevice& device, uint32_t queueFamilyIndex) : mDevice(device) {
    VkCommandPoolCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    // should be RESET|RANSIENT flag
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    createInfo.queueFamilyIndex = queueFamilyIndex;
    vkCreateCommandPool(mDevice, &createInfo, VKALLOC, &mPool);
    vkGetDeviceQueue(mDevice, queueFamilyIndex, 0, &mQueue);

    VkSemaphoreCreateInfo semaphoreCreateInfo { .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO };
    for (auto& semaphore : mSubmissionSignals) {
        vkCreateSemaphore(mDevice, &semaphoreCreateInfo, nullptr, &semaphore);
    }

    for (uint32_t index = 0; index < VK_MAX_COMMAND_BUFFERS; ++index) {
        mCommandBuffers[index].cmdBufferIndex = index;
    }
}

VulkanCommandPool::~VulkanCommandPool() {
    wait();
    gc();
    vkDestroyCommandPool(mDevice, mPool, VKALLOC);
    for (VkSemaphore semaphore : mSubmissionSignals) {
        vkDestroySemaphore(mDevice, semaphore, VKALLOC);
    }
}

VulkanCommandBuffer const& VulkanCommandPool::get() {

    if (mCurrentCmdBuffer) {
        // reset to null when flush() function is called
        return *mCurrentCmdBuffer;
    }

    while (mFreeCmdBufferCount == 0) {
        wait();
        gc();
    }
    // get an unused command buffer
    for (VulkanCommandBuffer& cmdBuffer : mCommandBuffers) {
        if (cmdBuffer.cmdbuffer == VK_NULL_HANDLE) {
            mCurrentCmdBuffer = &cmdBuffer;
            break;
        }
    }

    VR_ASSERT(mCurrentCmdBuffer);
     --mFreeCmdBufferCount;

    const VkCommandBufferAllocateInfo allocateInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = mPool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    vkAllocateCommandBuffers(mDevice, &allocateInfo, &mCurrentCmdBuffer->cmdbuffer);

    mCurrentCmdBuffer->queueSubmitFence = std::make_shared<VulkanFence>(mDevice);
    VkResult r = vkGetFenceStatus(mDevice, mCurrentCmdBuffer->queueSubmitFence->get());
    VR_ASSERT(r == VK_NOT_READY);

    const VkCommandBufferBeginInfo binfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    // begin of command recording
    vkBeginCommandBuffer(mCurrentCmdBuffer->cmdbuffer, &binfo);

    if (mObserver) {
        mObserver->onCommandBuffer(*mCurrentCmdBuffer);
    }

    return *mCurrentCmdBuffer;
}

bool VulkanCommandPool::flush() {

    if (mCurrentCmdBuffer == nullptr) {
        return false;
    }

    const int64_t index = mCurrentCmdBuffer - &mCommandBuffers[0];
    VkSemaphore renderFinished = mSubmissionSignals[index];
    // end of command recording
    vkEndCommandBuffer(mCurrentCmdBuffer->cmdbuffer);

    VkPipelineStageFlags waitDestStageMasks[2] = {
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    };
    // set wait and release signals
    VkSemaphore signals[2] = {
        VK_NULL_HANDLE,
        VK_NULL_HANDLE,
    };

    VkSubmitInfo submitInfo {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 0,
        .pWaitSemaphores = signals,
        .pWaitDstStageMask = waitDestStageMasks,
        .commandBufferCount = 1,
        .pCommandBuffers = &mCurrentCmdBuffer->cmdbuffer,
        .signalSemaphoreCount = 1u,
        .pSignalSemaphores = &renderFinished,
    };

    if (mRenderFinishedSignal) {
        signals[submitInfo.waitSemaphoreCount++] = mRenderFinishedSignal;
    }

    if (mAcquireImageSignal) {
        signals[submitInfo.waitSemaphoreCount++] = mAcquireImageSignal;
    }

    auto fence = mCurrentCmdBuffer->queueSubmitFence;
    // FIXME: should be called later?
    VkFence vkfence = fence->get();
    vkResetFences(mDevice, 1, &vkfence);
    VR_ASSERT(fence != nullptr);
    VkResult result = vkQueueSubmit(mQueue, 1, &submitInfo, fence->get());

    VR_ASSERT(result == VK_SUCCESS);
    // signal for previous frame 
    mRenderFinishedSignal = renderFinished;
    mAcquireImageSignal = VK_NULL_HANDLE;
    mCurrentCmdBuffer = nullptr;
    return true;
}

VkSemaphore VulkanCommandPool::getRenderFinishedSignal() {
    VkSemaphore semaphore = mRenderFinishedSignal;
    mRenderFinishedSignal = VK_NULL_HANDLE;
    return semaphore;
}

void VulkanCommandPool::setAcquireNextImageSignal(VkSemaphore acquireNext) {
    VR_ASSERT(mAcquireImageSignal == VK_NULL_HANDLE);
    mAcquireImageSignal = acquireNext;
}

void VulkanCommandPool::wait() {
    VkFence fences[VK_MAX_COMMAND_BUFFERS];
    uint32_t count = 0;
    for (auto& cmdBuffer : mCommandBuffers) {
        if (cmdBuffer.cmdbuffer != VK_NULL_HANDLE && mCurrentCmdBuffer != &cmdBuffer) {
            fences[count++] = cmdBuffer.queueSubmitFence->get();
        }
    }
    if (count > 0) {
        VkResult result = vkWaitForFences(mDevice, count, fences, VK_TRUE, UINT64_MAX);
        VR_ASSERT(result == VK_SUCCESS);
    }
}

void VulkanCommandPool::gc() {
    for (auto& cmdBuffer : mCommandBuffers) {
        if (cmdBuffer.cmdbuffer != VK_NULL_HANDLE) {
            auto fence = cmdBuffer.queueSubmitFence->get();
            VkResult result = vkWaitForFences(mDevice, 1, &fence, VK_TRUE, 0);
            if (result == VK_SUCCESS) {
                vkFreeCommandBuffers(mDevice, mPool, 1, &cmdBuffer.cmdbuffer);
                cmdBuffer.cmdbuffer = VK_NULL_HANDLE;
                cmdBuffer.queueSubmitFence.reset();
                ++mFreeCmdBufferCount;
            }
        }
    }
}

void VulkanCommandPool::updateFences() {
    for (auto& cmdBuffer : mCommandBuffers) {
        if (cmdBuffer.cmdbuffer != VK_NULL_HANDLE) {
            auto fence = cmdBuffer.queueSubmitFence;
            if (fence != nullptr) {
                VkResult status = vkGetFenceStatus(mDevice, fence->get());
            }
        }
    }
}

} // namespace backend
} // namespace VR
