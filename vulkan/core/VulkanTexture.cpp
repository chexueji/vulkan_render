#include "VulkanUtils.h"
#include "VulkanTexture.h"
#include "VulkanMemoryPool.h"

namespace VR {
namespace backend {

// cpu to gpu layout
static void transitionImageLayout(VkCommandBuffer cmd, VkImage image,
        VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t miplevel,
        uint32_t layerCount, uint32_t levelCount, VkImageAspectFlags aspect) {
    if (oldLayout == newLayout) {
        return;
    }
    VkImageMemoryBarrier barrier = {};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.baseMipLevel = miplevel;
    barrier.subresourceRange.levelCount = levelCount;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = layerCount;
    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;
    switch (newLayout) {
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
        case VK_IMAGE_LAYOUT_GENERAL:
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            break;

        // allow blitting from the swap chain
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = 0;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            break;

        default:
           VR_ERROR("Unsupported layout transition.");
    }
    vkCmdPipelineBarrier(cmd, sourceStage, destinationStage, 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

VulkanTexture::VulkanTexture(VulkanContext& context, SamplerType target, uint8_t levels, TextureFormat format, uint8_t samples, uint32_t w, uint32_t h, uint32_t depth,
        TextureUsage usage, VulkanMemoryPool& memoryPool, VkComponentMapping swizzle) :
        mTarget(target), mMipLevels(levels), mSamples(samples), mWidth(w), mHeight(h), mDepth(depth), mFormat(format), mUsage(usage),
        // not support 24-bit depth
        mVkFormat(format == TextureFormat::DEPTH24 ? context.depthFormat : backend::getVkFormat(format)), mSwizzle(swizzle), mContext(context), mMemoryPool(memoryPool) {

    VkImageCreateInfo imageInfo {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = target == SamplerType::SAMPLER_3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D,
        .format = mVkFormat,
        .extent = { w, h, depth },
        .mipLevels = levels,
        .arrayLayers = 1,
        .samples = (VkSampleCountFlagBits) samples,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = 0
    };
    if (target == SamplerType::SAMPLER_CUBEMAP) {
        imageInfo.arrayLayers = 6;
        imageInfo.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    }
    if (target == SamplerType::SAMPLER_2D_ARRAY) {
        imageInfo.arrayLayers = depth;
        imageInfo.extent.depth = 1;
    }

    // for blit
    const VkImageUsageFlags blittFlag = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;

    if (any(usage & TextureUsage::SAMPLEABLE)) {
        imageInfo.usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
    }
    if (any(usage & TextureUsage::COLOR_ATTACHMENT)) {
        imageInfo.usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | blittFlag;
        if (any(usage & TextureUsage::SUBPASS_INPUT)) {
            imageInfo.usage |= VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        }
    }
    if (any(usage & TextureUsage::STENCIL_ATTACHMENT)) {
        imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    if (any(usage & TextureUsage::UPLOADABLE)) {
        imageInfo.usage |= blittFlag;
    }
    if (any(usage & TextureUsage::DEPTH_ATTACHMENT)) {
        imageInfo.usage |= blittFlag;
        imageInfo.usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }

    VkResult error = vkCreateImage(context.device, &imageInfo, VKALLOC, &mImage);
    VR_VK_ASSERT(error == VK_SUCCESS , "Unable to create image.");

    // allocate memory for the VkImage and bind it
    VkMemoryRequirements memReqs = {};
    vkGetImageMemoryRequirements(context.device, mImage, &memReqs);
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = selectMemoryType(context, memReqs.memoryTypeBits,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
    };
    error = vkAllocateMemory(context.device, &allocInfo, nullptr, &mImageMemory);
    VR_VK_ASSERT(error == VK_SUCCESS, "Unable to allocate image memory.");

    error = vkBindImageMemory(context.device, mImage, mImageMemory, 0);
    VR_VK_ASSERT(error == VK_SUCCESS, "Unable to bind image.");

    if (any(usage & TextureUsage::DEPTH_ATTACHMENT)) {
        mAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    } else if(any(usage & TextureUsage::STENCIL_ATTACHMENT)) {
        mAspect = VK_IMAGE_ASPECT_STENCIL_BIT;
    } else {
        mAspect = VK_IMAGE_ASPECT_COLOR_BIT;
    }

    // set primary image view
    mPrimaryViewRange.aspectMask = mAspect;
    mPrimaryViewRange.baseMipLevel = 0;
    mPrimaryViewRange.levelCount = levels;
    mPrimaryViewRange.baseArrayLayer = 0;
    if (target == SamplerType::SAMPLER_CUBEMAP) {
        mViewType = VK_IMAGE_VIEW_TYPE_CUBE;
        mPrimaryViewRange.layerCount = 6;
    } else if (target == SamplerType::SAMPLER_2D_ARRAY) {
        mViewType = VK_IMAGE_VIEW_TYPE_2D_ARRAY;
        mPrimaryViewRange.layerCount = depth;
    } else if (target == SamplerType::SAMPLER_3D) {
        mViewType = VK_IMAGE_VIEW_TYPE_3D;
        mPrimaryViewRange.layerCount = 1;
    } else {
        mViewType = VK_IMAGE_VIEW_TYPE_2D;
        mPrimaryViewRange.layerCount = 1;
    }

    // create the primary image view
    getImageView(mPrimaryViewRange);

    // transition the layout
    if (any(usage & (TextureUsage::COLOR_ATTACHMENT | TextureUsage::DEPTH_ATTACHMENT))) {
        const uint32_t layers = mPrimaryViewRange.layerCount;
        transitionImageLayout(mContext.commandpool->get().cmdbuffer, mImage, VK_IMAGE_LAYOUT_UNDEFINED, getTextureLayout(usage), 0, layers, levels, mAspect);
    }
}

VulkanTexture::~VulkanTexture() {
    vkDestroyImage(mContext.device, mImage, VKALLOC);
    vkFreeMemory(mContext.device, mImageMemory, VKALLOC);
     for (auto& imageView : mCachedImageViews) {
         vkDestroyImageView(mContext.device, imageView.second, VKALLOC);
     }
}

void VulkanTexture::update2DImage(const PixelBufferDescriptor& data, uint32_t width, uint32_t height, int miplevel) {
    update3DImage(std::move(data), width, height, 1, miplevel);
}

void VulkanTexture::download2DImage(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t depth, int miplevel, PixelBufferDescriptor& hostData) {
  
    // only support 4 channels
    VR_ASSERT(pbd.buffer != nullptr && hostData.size != 0);
    VulkanBufferMemory const* buffer = mMemoryPool.acquireBuffer(hostData.size);
    const VkCommandBuffer cmdbuffer = mContext.commandpool->get().cmdbuffer;
    
    // copy image to buffer
    copyImageToBuffer(cmdbuffer, mImage, buffer->buffer, width, height, depth, nullptr, miplevel);
    // wait for rendering to finish
    waitForIdle(mContext);
    
    void* mapped = nullptr;
    vmaMapMemory(mContext.allocator, buffer->memory, &mapped);
    ::memcpy(hostData.buffer, mapped, hostData.size);
    vmaUnmapMemory(mContext.allocator, buffer->memory);
    vmaInvalidateAllocation(mContext.allocator, buffer->memory, 0, hostData.size);
    
    // waitForIdle(mContext);
}

void VulkanTexture::update3DImage(const PixelBufferDescriptor& data, uint32_t width, uint32_t height, uint32_t depth, int miplevel) {
    VR_ASSERT(width <= this->mWidth && height <= this->mHeight && depth <= this->mDepth);
    const PixelBufferDescriptor* hostData = &data;
    PixelBufferDescriptor reshapedData;

    // only support 4 channels 
    const VkFormat hostFormat = backend::getVkFormat(hostData->format, hostData->type);
    const VkFormat deviceFormat = getVkFormatLinear(mVkFormat);

    if (hostFormat != deviceFormat && hostFormat != VK_FORMAT_UNDEFINED) {
        updateWithBlitImage(*hostData, width, height, depth, miplevel);
    } else {
        updateWithCopyBuffer(*hostData, width, height, depth, miplevel);
    }
}

void VulkanTexture::updateWithCopyBuffer(const PixelBufferDescriptor& hostData, uint32_t width, uint32_t height, uint32_t depth, int miplevel) {
    void* mapped = nullptr;
    VulkanBufferMemory const* buffer = mMemoryPool.acquireBuffer(hostData.size);
    vmaMapMemory(mContext.allocator, buffer->memory, &mapped);
    ::memcpy(mapped, hostData.buffer, hostData.size);
    vmaUnmapMemory(mContext.allocator, buffer->memory);
    vmaFlushAllocation(mContext.allocator, buffer->memory, 0, hostData.size);

    const VkCommandBuffer cmdbuffer = mContext.commandpool->get().cmdbuffer;
    transitionImageLayout(cmdbuffer, mImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, miplevel, 1, 1, mAspect);
    copyBufferToImage(cmdbuffer, buffer->buffer, mImage, width, height, depth, nullptr, miplevel);
    transitionImageLayout(cmdbuffer, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, getTextureLayout(mUsage), miplevel, 1, 1, mAspect);
}

void VulkanTexture::updateWithBlitImage(const PixelBufferDescriptor& hostData, uint32_t width, uint32_t height, uint32_t depth, int miplevel) {
    void* mapped = nullptr;
    VulkanImageMemory const* imageMemory = mMemoryPool.acquireImage(hostData.format, hostData.type, width, height);
    vmaMapMemory(mContext.allocator, imageMemory->memory, &mapped);
    ::memcpy(mapped, hostData.buffer, hostData.size);
    vmaUnmapMemory(mContext.allocator, imageMemory->memory);
    vmaFlushAllocation(mContext.allocator, imageMemory->memory, 0, hostData.size);

    const VkCommandBuffer cmdbuffer = mContext.commandpool->get().cmdbuffer;

    // 3D images and cubemaps, for blit
    const int layer = 0;

    const VkOffset3D rect[2] { {0, 0, 0}, {int32_t(width), int32_t(height), 1} };

    const VkImageBlit blitRegions[1] = {{
        .srcSubresource = { mAspect, 0, 0, 1 },
        .srcOffsets = { rect[0], rect[1] },
        .dstSubresource = { mAspect, uint32_t(miplevel), layer, 1 },
        .dstOffsets = { rect[0], rect[1] }
    }};

    transitionImageLayout(cmdbuffer, imageMemory->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, miplevel, 1, 1, mAspect);
    transitionImageLayout(cmdbuffer, mImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, miplevel, 1, 1, mAspect);
    vkCmdBlitImage(cmdbuffer, imageMemory->image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, blitRegions, VK_FILTER_NEAREST); // filter: or VK_FILTER_LINEAR
    transitionImageLayout(cmdbuffer, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, getTextureLayout(mUsage), miplevel, 1, 1, mAspect);
}

void VulkanTexture::updateCubeImage(const PixelBufferDescriptor& data, const FaceOffsets& faceOffsets, int miplevel) {
    VR_ASSERT(this->mTarget == SamplerType::SAMPLER_CUBEMAP);
    VR_ASSERT(getBytesPerPixel(format) == 4);

    const void* cpuData = data.buffer;
    const uint32_t numSrcBytes = data.size;
    const uint32_t numDstBytes = numSrcBytes;

    VulkanBufferMemory const* buffer = mMemoryPool.acquireBuffer(numDstBytes);
    void* mapped;
    vmaMapMemory(mContext.allocator, buffer->memory, &mapped);
    ::memcpy(mapped, cpuData, numSrcBytes);
    vmaUnmapMemory(mContext.allocator, buffer->memory);
    vmaFlushAllocation(mContext.allocator, buffer->memory, 0, numDstBytes);

    const VkCommandBuffer cmdbuffer = mContext.commandpool->get().cmdbuffer;
    const uint32_t width = std::max(1u, this->mWidth >> miplevel);
    const uint32_t height = std::max(1u, this->mHeight >> miplevel);

    transitionImageLayout(cmdbuffer, mImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, miplevel, 6, 1, mAspect);
    copyBufferToImage(cmdbuffer, buffer->buffer, mImage, width, height, 1, &faceOffsets, miplevel);
    transitionImageLayout(cmdbuffer, mImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, getTextureLayout(mUsage), miplevel, 6, 1, mAspect);
}

void VulkanTexture::setPrimaryRange(uint32_t minMiplevel, uint32_t maxMiplevel) {
    maxMiplevel = std::min(int(maxMiplevel), int(this->mMipLevels - 1));
    mPrimaryViewRange.baseMipLevel = minMiplevel;
    mPrimaryViewRange.levelCount = maxMiplevel - minMiplevel + 1;
    getImageView(mPrimaryViewRange);
}

VkImageView VulkanTexture::getImageView(VkImageSubresourceRange range, bool force2D) const {
    
     std::string imageKey = "aspect_" + std::to_string(range.aspectMask) +
                            "_basemiplevel_" + std::to_string(range.baseMipLevel) + "_levelcount_" + std::to_string(range.levelCount) +
                            "_basearraylayer_" + std::to_string(range.baseArrayLayer) + "_layercount_" + std::to_string(range.layerCount);
    
    auto iter = mCachedImageViews.find(imageKey);
    if (iter != mCachedImageViews.end()) {
        return iter->second;
    }

    VkImageViewCreateInfo viewInfo = {
    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .image = mImage,
    .viewType = force2D ? VK_IMAGE_VIEW_TYPE_2D : mViewType,
    .format = mVkFormat,
    .components = mSwizzle,
    .subresourceRange = range
    };
    VkImageView imageView;
    vkCreateImageView(mContext.device, &viewInfo, VKALLOC, &imageView);

    mCachedImageViews.emplace(std::make_pair(imageKey, imageView));
    return imageView;
}

VkImageView VulkanTexture::getPrimaryImageView() const {
    return getImageView(mPrimaryViewRange);
}

void VulkanTexture::copyBufferToImage(VkCommandBuffer cmd, VkBuffer buffer, VkImage image,
        uint32_t width, uint32_t height, uint32_t depth, FaceOffsets const* faceOffsets, uint32_t miplevel) {
    VkExtent3D extent { width, height, depth };
    if (mTarget == SamplerType::SAMPLER_CUBEMAP) {
        VR_ASSERT(faceOffsets);
        VkBufferImageCopy regions[6] = {{}};
        for (size_t face = 0; face < 6; face++) {
            auto& region = regions[face];
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.baseArrayLayer = face;
            region.imageSubresource.layerCount = 1;
            region.imageSubresource.mipLevel = miplevel;
            region.imageExtent = extent;
            region.bufferOffset = faceOffsets->offsets[face];
            region.bufferRowLength = 0; // equal to imageExtent.width
            region.bufferImageHeight = 0; // equal to imageExtent.height
        }
        vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 6, regions);
        return;
    }
    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = miplevel;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = extent;
    region.bufferRowLength = 0; // equal to imageExtent.width
    region.bufferImageHeight = 0; // equal to imageExtent.height
    vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

void VulkanTexture::copyImageToBuffer(VkCommandBuffer cmd, VkImage image, VkBuffer buffer,
        uint32_t width, uint32_t height, uint32_t depth, FaceOffsets const* faceOffsets, uint32_t miplevel) {
    VkExtent3D extent { width, height, depth };
    if (mTarget == SamplerType::SAMPLER_CUBEMAP) {
        VR_ASSERT(faceOffsets);
        VkBufferImageCopy regions[6] = {{}};
        for (size_t face = 0; face < 6; face++) {
            auto& region = regions[face];
            region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            region.imageSubresource.baseArrayLayer = face;
            region.imageSubresource.layerCount = 1;
            region.imageSubresource.mipLevel = miplevel;
            region.imageExtent = extent;
            region.bufferOffset = faceOffsets->offsets[face];
            region.bufferRowLength = 0; // equal to imageExtent.width
            region.bufferImageHeight = 0; // equal to imageExtent.height
        }
        vkCmdCopyImageToBuffer(cmd, image, getTextureLayout(mUsage), buffer, 6, regions);
        return;
    }
    VkBufferImageCopy region = {};
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = miplevel;
    region.imageSubresource.layerCount = 1;
    region.imageExtent = extent;
    region.bufferRowLength = 0; // equal to imageExtent.width
    region.bufferImageHeight = 0; // equal to imageExtent.height
    vkCmdCopyImageToBuffer(cmd, image, getTextureLayout(mUsage), buffer, 1, &region);
}

} // namespace backend
} // namespace VR
