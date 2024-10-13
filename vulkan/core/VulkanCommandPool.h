#ifndef VULKAN_COMMANDS_H
#define VULKAN_COMMANDS_H

#include <memory>
#include "NonCopyable.h"
#include "VulkanWrapper.h"
#include "VulkanMacros.h"

namespace VR {
namespace backend {

class VulkanFence;

struct VulkanCommandBuffer {
    VulkanCommandBuffer() {}
    VulkanCommandBuffer(VulkanCommandBuffer const&) = delete;
    VulkanCommandBuffer& operator=(VulkanCommandBuffer const&) = delete;
    VkCommandBuffer cmdbuffer = VK_NULL_HANDLE;
    std::shared_ptr<VulkanFence> queueSubmitFence;
    uint32_t cmdBufferIndex = 0;
};

class CommandBufferObserver {
public:
    virtual void onCommandBuffer(const VulkanCommandBuffer& cmdbuffer) = 0;
    virtual ~CommandBufferObserver();
};

class VulkanCommandPool : public NonCopyable {
    public:
        VulkanCommandPool(const VkDevice& device, uint32_t queueFamilyIndex);
        virtual ~VulkanCommandPool();

        VulkanCommandBuffer const& get();
        bool flush();
        void wait();
        void gc();
        void updateFences();
        void setObserver(CommandBufferObserver* observer) { mObserver = observer; }
        VkSemaphore getRenderFinishedSignal();
        void setAcquireNextImageSignal(VkSemaphore next);

    private:
        const VkDevice& mDevice;
        VkQueue mQueue;
        VkCommandPool mPool;
        VulkanCommandBuffer* mCurrentCmdBuffer = nullptr;
        VkSemaphore mRenderFinishedSignal = {};
        VkSemaphore mAcquireImageSignal = {};
        VulkanCommandBuffer mCommandBuffers[VK_MAX_COMMAND_BUFFERS] = {};
        VkSemaphore mSubmissionSignals[VK_MAX_COMMAND_BUFFERS] = {};
        size_t mFreeCmdBufferCount = VK_MAX_COMMAND_BUFFERS;
        CommandBufferObserver* mObserver = nullptr;
};

} // namespace backend
} // namespace VR

#endif // VULKAN_COMMANDS_H
