#ifndef VULKAN_TEXTURE_H
#define VULKAN_TEXTURE_H

#include <map>
#include <string>
#include "VulkanBuffer.h"
#include "BufferDescriptor.h"

namespace VR {
namespace backend {

class VulkanTexture : public NonCopyable {
public:
    VulkanTexture(VulkanContext& context, SamplerType target, uint8_t levels, TextureFormat format, uint8_t samples, uint32_t w, uint32_t h, uint32_t depth,
                  TextureUsage usage, VulkanMemoryPool& memoryPool, VkComponentMapping swizzle = {});
    virtual ~VulkanTexture();
    void update2DImage(const PixelBufferDescriptor& data, uint32_t width, uint32_t height, int miplevel);
    void download2DImage(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t depth, int miplevel, PixelBufferDescriptor& pbd);
    void update3DImage(const PixelBufferDescriptor& data, uint32_t width, uint32_t height, uint32_t depth, int miplevel);
    void updateCubeImage(const PixelBufferDescriptor& data, const FaceOffsets& faceOffsets, int miplevel);

    VkImageView getPrimaryImageView() const;
    void setPrimaryRange(uint32_t minMiplevel, uint32_t maxMiplevel);
    VkImageView getImageView(VkImageSubresourceRange range, bool force2D = false) const;
    // get image view with one level and layer
    VkImageView getImageView(int level, int layer, VkImageAspectFlags aspect) {
        return getImageView({ .aspectMask = aspect, .baseMipLevel = uint32_t(level), .levelCount = uint32_t(1), .baseArrayLayer = uint32_t(layer), .layerCount = uint32_t(1),
        }, true);
    }
    // texture properties
    VkFormat vkFormat() const { return mVkFormat; }
    VkImage vkImage() const { return mImage; }
    TextureUsage usage() const { return mUsage; }
    uint32_t width() const { return mWidth; }
    uint32_t height() const { return mHeight; }
    uint32_t depth() const { return mDepth; }
    uint8_t mipLevels() const { return mMipLevels; }
    uint8_t samples() const { return mSamples; }
    TextureFormat format() const { return mFormat; }
    SamplerType target() const { return mTarget; }
    VkDeviceMemory deviceMemory() const { return mImageMemory;}
    
private:

    void copyBufferToImage(VkCommandBuffer cmdbuffer, VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t depth, FaceOffsets const* faceOffsets, uint32_t miplevel);
    void copyImageToBuffer(VkCommandBuffer cmd, VkImage image, VkBuffer buffer, uint32_t width, uint32_t height, uint32_t depth, FaceOffsets const* faceOffsets, uint32_t miplevel);
    void updateWithCopyBuffer(const PixelBufferDescriptor& hostData, uint32_t width, uint32_t height, uint32_t depth, int miplevel);
    void updateWithBlitImage(const PixelBufferDescriptor& hostData, uint32_t width, uint32_t height, uint32_t depth, int miplevel);

    VulkanContext& mContext;
    VulkanMemoryPool& mMemoryPool;
    
    const VkFormat mVkFormat;
    const VkComponentMapping mSwizzle;
    VkImageViewType mViewType;
    VkImage mImage = VK_NULL_HANDLE;
    VkDeviceMemory mImageMemory = VK_NULL_HANDLE;
    VkImageSubresourceRange mPrimaryViewRange;
    mutable std::map<std::string, VkImageView> mCachedImageViews{};
    VkImageAspectFlags mAspect;

private:
    uint32_t mWidth;
    uint32_t mHeight;
    uint32_t mDepth;
    SamplerType mTarget;
    uint8_t mMipLevels;
    uint8_t mSamples;
    TextureFormat mFormat;
    TextureUsage mUsage;    
};

} // namespace backend
} // namespace VR

#endif // VULKAN_TEXTURE_H
