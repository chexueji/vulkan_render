#ifndef VULKAN_MEMORY_POOL_H
#define VULKAN_MEMORY_POOL_H

#include <map>
#include <unordered_set>
#include "VulkanContext.h"

namespace VR {
namespace backend {

struct VulkanBufferMemory {
    VmaAllocation memory;
    VkBuffer buffer;
    uint32_t capacity;
    mutable uint64_t lastAccessed;
};

struct VulkanImageMemory {
    VkFormat format;
    uint32_t width;
    uint32_t height;
    mutable uint64_t lastAccessed;
    VmaAllocation memory;
    VkImage image;
};

class VulkanMemoryPool : public NonCopyable {
public:
    VulkanMemoryPool(VulkanContext& context) : mContext(context) {}

    VulkanBufferMemory const* acquireBuffer(uint32_t numBytes);
    VulkanImageMemory const* acquireImage(PixelDataFormat format, PixelDataType type, uint32_t width, uint32_t height);
    void gc();
    void reset();

private:
    VulkanContext& mContext;
    std::multimap<uint32_t, VulkanBufferMemory const*> mFreeBuffers;
    std::unordered_set<VulkanBufferMemory const*> mUsedBuffers;
    std::unordered_set<VulkanImageMemory const*> mFreeImages;
    std::unordered_set<VulkanImageMemory const*> mUsedImages;
    // for LRU 
    uint64_t mCurrentFrame = 0;
};

} // namespace backend
} // namespace VR

#endif // VULKAN_MEMORY_POOL_H
