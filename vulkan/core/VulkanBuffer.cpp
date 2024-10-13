#include "VulkanBuffer.h"
#include "VulkanMemoryPool.h"
#include "VulkanCommandPool.h"

namespace VR {
namespace backend {

VulkanBuffer::VulkanBuffer(VulkanContext& context, VulkanMemoryPool& memoryPool,
        VkBufferUsageFlags usage, uint32_t numBytes) : mContext(context), mMemoryPool(memoryPool), mByteCount(numBytes) {

    VkBufferCreateInfo bufferInfo {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = numBytes,
        .usage = usage | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT
    };
    VmaAllocationCreateInfo allocInfo {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY
    };
    vmaCreateBuffer(context.allocator, &bufferInfo, &allocInfo, &mGpuBuffer, &mGpuMemory, nullptr);
}

VulkanBuffer::~VulkanBuffer() {
    vmaDestroyBuffer(mContext.allocator, mGpuBuffer, mGpuMemory);
}

void VulkanBuffer::upload(const void* cpuData, uint32_t byteOffset, uint32_t numBytes) {
    VR_ASSERT(byteOffset == 0);
    VulkanBufferMemory const* buffer = mMemoryPool.acquireBuffer(numBytes);
    VR_ASSERT(buffer->memory != nullptr);
    void* mapped;
    vmaMapMemory(mContext.allocator, buffer->memory, &mapped);
    ::memcpy(mapped, cpuData, numBytes);
    vmaUnmapMemory(mContext.allocator, buffer->memory);
    vmaFlushAllocation(mContext.allocator, buffer->memory, byteOffset, numBytes);

    const VkCommandBuffer cmdbuffer = mContext.commandpool->get().cmdbuffer;

    VkBufferCopy region { .size = numBytes };
    vkCmdCopyBuffer(cmdbuffer, buffer->buffer, mGpuBuffer, 1, &region);

    VkBufferMemoryBarrier barrier {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT |
                         VK_ACCESS_TRANSFER_WRITE_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = mGpuBuffer,
        .size = VK_WHOLE_SIZE
    };

    vkCmdPipelineBarrier(cmdbuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void VulkanBuffer::download(void* cpuData, uint32_t byteOffset, uint32_t numBytes) {
    VR_ASSERT(byteOffset == 0);
    VulkanBufferMemory const* buffer = mMemoryPool.acquireBuffer(numBytes);
    VR_ASSERT(buffer->memory != nullptr);

    const VkCommandBuffer cmdbuffer = mContext.commandpool->get().cmdbuffer;

    VkBufferCopy region { .size = numBytes };
    vkCmdCopyBuffer(cmdbuffer, mGpuBuffer, buffer->buffer, 1, &region);

    VkBufferMemoryBarrier barrier {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = buffer->buffer,
        .size = VK_WHOLE_SIZE
    };

    vkCmdPipelineBarrier(cmdbuffer,
             VK_PIPELINE_STAGE_TRANSFER_BIT,
             VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT, // or VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT
            0, 0, nullptr, 1, &barrier, 0, nullptr);
    
    waitForIdle(mContext);
    
    void* mapped;
    vmaMapMemory(mContext.allocator, buffer->memory, &mapped);
    ::memcpy(cpuData, mapped, numBytes);
    vmaUnmapMemory(mContext.allocator, buffer->memory);
}

} // namespace backend
} // namespace VR
