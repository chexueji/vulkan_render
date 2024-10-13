#ifndef VULKAN_BUFFER_H
#define VULKAN_BUFFER_H

#include "VulkanContext.h"

namespace VR {
namespace backend {

class VulkanBuffer : public NonCopyable {
public:
    VulkanBuffer(VulkanContext& context, VulkanMemoryPool& memoryPool, VkBufferUsageFlags usage, uint32_t numBytes);
    virtual ~VulkanBuffer();
    void upload(const void* cpuData, uint32_t byteOffset, uint32_t numBytes);
    void download(void* cpuData, uint32_t byteOffset, uint32_t numBytes);
    VkBuffer getGpuBuffer() const { return mGpuBuffer; }
    uint32_t getByteCount() const { return mByteCount; }
private:
    uint32_t mByteCount{};
    VulkanContext& mContext;
    VulkanMemoryPool& mMemoryPool;
    VmaAllocation mGpuMemory = VK_NULL_HANDLE;
    VkBuffer mGpuBuffer = VK_NULL_HANDLE;
};

} // namespace backend
} // namespace VR

#endif // VULKAN_BUFFER_H
