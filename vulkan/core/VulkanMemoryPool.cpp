#include "VulkanUtils.h"
#include "VulkanMemoryPool.h"

namespace VR {
namespace backend {

VulkanBufferMemory const* VulkanMemoryPool::acquireBuffer(uint32_t numBytes) {
    auto iter = mFreeBuffers.lower_bound(numBytes);
    if (iter != mFreeBuffers.end()) {
        auto buffer = iter->second;
        mFreeBuffers.erase(iter);
        mUsedBuffers.insert(buffer);
        return buffer;
    }
    
    VulkanBufferMemory* buffer = new VulkanBufferMemory({
        .memory = VK_NULL_HANDLE,
        .buffer = VK_NULL_HANDLE,
        .capacity = numBytes,
        .lastAccessed = mCurrentFrame,
    });

    mUsedBuffers.insert(buffer);
    VkBufferCreateInfo bufferInfo {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = numBytes,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    VmaAllocationCreateInfo allocInfo {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_CPU_ONLY // note that memory allocated with #VMA_MEMORY_USAGE_CPU_ONLY is guaranteed to be 'HOST_COHERENT', for host map
    };

    vmaCreateBuffer(mContext.allocator, &bufferInfo, &allocInfo, &buffer->buffer, &buffer->memory, nullptr);
    
    return buffer;
}

VulkanImageMemory const* VulkanMemoryPool::acquireImage(PixelDataFormat format, PixelDataType type, uint32_t width, uint32_t height) {
    const VkFormat vkformat = getVkFormat(format, type);
    for (auto image : mFreeImages) {
        if (image->format == vkformat && image->width == width && image->height == height) {
            mFreeImages.erase(image);
            mUsedImages.insert(image);
            return image;
        }
    }

    VulkanImageMemory* image = new VulkanImageMemory({
        .format = vkformat,
        .width = width,
        .height = height,
        .lastAccessed = mCurrentFrame,
    });

    mUsedImages.insert(image);

    const VkImageCreateInfo imageInfo = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = vkformat,
        .extent = { width, height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT
    };

    const VmaAllocationCreateInfo allocInfo {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = VMA_MEMORY_USAGE_CPU_TO_GPU
    };

    const VkResult result = vmaCreateImage(mContext.allocator, &imageInfo, &allocInfo, &image->image, &image->memory, nullptr);
    VR_ASSERT(result == VK_SUCCESS);

    return image;
}

void VulkanMemoryPool::gc() {
    if (++mCurrentFrame <= VK_MAX_COMMAND_BUFFERS) {
        return;
    }
    const uint64_t gcTime = mCurrentFrame - VK_MAX_COMMAND_BUFFERS;

    // destroy buffers that have not been used for several frames
    std::multimap<uint32_t, VulkanBufferMemory const*> freeBuffers;
    freeBuffers.swap(mFreeBuffers);
    for (auto pair : freeBuffers) {
        if (pair.second->lastAccessed < gcTime) {
            vmaDestroyBuffer(mContext.allocator, pair.second->buffer, pair.second->memory);
            delete pair.second;
        } else {
            mFreeBuffers.insert(pair);
        }
    }

    // return buffers that are no longer being used by any command buffer
    std::unordered_set<VulkanBufferMemory const*> usedBuffers;
    usedBuffers.swap(mUsedBuffers);
    for (auto buffer : usedBuffers) {
        if (buffer->lastAccessed < gcTime) {
            buffer->lastAccessed = mCurrentFrame;
            mFreeBuffers.insert(std::make_pair(buffer->capacity, buffer));
        } else {
            mUsedBuffers.insert(buffer);
        }
    }

    // destroy images that have not been used for several frames
    std::unordered_set<VulkanImageMemory const*> freeImages;
    freeImages.swap(mFreeImages);
    for (auto image : freeImages) {
        if (image->lastAccessed < gcTime) {
            vmaDestroyImage(mContext.allocator, image->image, image->memory);
            delete image;
        } else {
            mFreeImages.insert(image);
        }
    }

    // return images that are no longer being used by any command buffer
    std::unordered_set<VulkanImageMemory const*> usedImages;
    usedImages.swap(mUsedImages);
    for (auto image : usedImages) {
        if (image->lastAccessed < gcTime) {
            image->lastAccessed = mCurrentFrame;
            mFreeImages.insert(image);
        } else {
            mUsedImages.insert(image);
        }
    }
}

void VulkanMemoryPool::reset() {
    for (auto buffer : mUsedBuffers) {
        vmaDestroyBuffer(mContext.allocator, buffer->buffer, buffer->memory);
        delete buffer;
    }
    mUsedBuffers.clear();

    for (auto pair : mFreeBuffers) {
        vmaDestroyBuffer(mContext.allocator, pair.second->buffer, pair.second->memory);
        delete pair.second;
    }
    mFreeBuffers.clear();

    for (auto image : mUsedImages) {
        vmaDestroyImage(mContext.allocator, image->image, image->memory);
        delete image;
    }
    mUsedBuffers.clear();

    for (auto image : mFreeImages) {
        vmaDestroyImage(mContext.allocator, image->image, image->memory);
        delete image;
    }
    mFreeBuffers.clear();
}

} // namespace backend
} // namespace VR
