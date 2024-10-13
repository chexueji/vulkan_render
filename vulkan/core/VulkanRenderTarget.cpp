#include "VulkanRenderTarget.h"

namespace VR {
namespace backend {

// helper functions
static void flipVertically(VkRect2D* rect, uint32_t framebufferHeight) {
    rect->offset.y = framebufferHeight - rect->offset.y - rect->extent.height;
}

static void flipVertically(VkViewport* rect, uint32_t framebufferHeight) {
    rect->y = framebufferHeight - rect->y - rect->height;
}

static void clampToFramebuffer(VkRect2D* rect, uint32_t fbWidth, uint32_t fbHeight) {
    int32_t x = std::max(rect->offset.x, 0);
    int32_t y = std::max(rect->offset.y, 0);
    int32_t right = std::min(rect->offset.x + (int32_t) rect->extent.width, (int32_t) fbWidth);
    int32_t top = std::min(rect->offset.y + (int32_t) rect->extent.height, (int32_t) fbHeight);
    rect->offset.x = std::min(x, (int32_t) fbWidth);
    rect->offset.y = std::min(y, (int32_t) fbHeight);
    rect->extent.width = std::max(right - x, 0);
    rect->extent.height = std::max(top - y, 0);
}

// attachments
VulkanAttachment createAttachment(VulkanAttachment spec) {
    if (spec.texture == nullptr) {
        return spec;
    }
    
    VulkanAttachment tmp;
    tmp.format = spec.texture->vkFormat();
    tmp.image = spec.texture->vkImage();
    tmp.view = spec.texture->getPrimaryImageView();
    tmp.memory = spec.texture->deviceMemory();
    tmp.texture = spec.texture;
    tmp.layout = getTextureLayout(spec.texture->usage());
    tmp.baseMipLevel = spec.baseMipLevel;
    tmp.baseLayer = spec.baseLayer;
    
    return tmp;
}

// Render target
VulkanRenderTarget::VulkanRenderTarget(VulkanContext& context, VulkanMemoryPool& memoryPool) : mContext(context), mMemoryPool(memoryPool), mWidth(0), mHeight(0),
                                          mSamples(1), mOffscreen(false) {}

VulkanRenderTarget::VulkanRenderTarget(VulkanContext& context, VulkanMemoryPool& memoryPool, uint32_t width, uint32_t height, uint8_t samples, VulkanAttachment color[MAX_SUPPORTED_RENDER_TARGET_COUNT],
                                        VulkanAttachment depthStencil[2]) : mContext(context), mMemoryPool(memoryPool), mWidth(width), mHeight(height), mSamples(samples), mOffscreen(false) {
    // color attachment
    for (int index = 0; index < MAX_SUPPORTED_RENDER_TARGET_COUNT; index++) {
        const VulkanAttachment& colorSpec = color[index];
        mColorAttachments[index] = createAttachment(colorSpec);
        VulkanTexture* colorTexture = colorSpec.texture;
        if (colorTexture == nullptr) {
            continue;
        }
        mColorAttachments[index].view = colorTexture->getImageView(colorSpec.baseMipLevel, colorSpec.baseLayer, VK_IMAGE_ASPECT_COLOR_BIT);
    }
    
    // depth attachment
    const VulkanAttachment& depthSpec = depthStencil[0];
    mDepthAttachment = createAttachment(depthSpec);
    VulkanTexture* depthTexture = mDepthAttachment.texture;
    if (depthTexture) {
        mDepthAttachment.view = depthTexture->getImageView(mDepthAttachment.baseMipLevel, mDepthAttachment.baseLayer, VK_IMAGE_ASPECT_DEPTH_BIT);
    }

    if (samples == 1) {
        return;
    }
    
    // set mass color and depth textures
    const int level = 1;
    const int depth = 1;

    // msaa textures for color attachments
    for (int index = 0; index < MAX_SUPPORTED_RENDER_TARGET_COUNT; index++) {
        const VulkanAttachment& spec = color[index];
        VulkanTexture* texture = spec.texture;
        if (texture && texture->samples() == 1) {
            VulkanTexture* msTexture = new VulkanTexture(context, texture->target(), level, texture->format(), samples, width, height, depth, texture->usage(), memoryPool);
            
            VulkanAttachment colorAttaSpec;
            colorAttaSpec.texture = msTexture;
            mMsaaColorAttachments[index] = createAttachment(colorAttaSpec);
            mMsaaColorAttachments[index].view = msTexture->getImageView(0, 0, VK_IMAGE_ASPECT_COLOR_BIT);
        }
        if (texture && texture->samples() > 1) {
            mMsaaColorAttachments[index] = mColorAttachments[index];
        }
    }

    if (depthTexture == nullptr) {
        return;
    }

    if (depthTexture->samples() > 1) {
        mMsaaDepthAttachment = mDepthAttachment;
        return;
    }

    // msaa texture for the depth attachment
    VulkanTexture* msaaDepthTexture = new VulkanTexture(context, depthTexture->target(), level, depthTexture->format(), samples, width, height, depth, depthTexture->usage(), memoryPool);
    
    VulkanAttachment msaaAttaSpec;
    msaaAttaSpec.format = {};
    msaaAttaSpec.image = {};
    msaaAttaSpec.view = {},
    msaaAttaSpec.memory = {},
    msaaAttaSpec.texture = msaaDepthTexture;
    msaaAttaSpec.layout = {};
    msaaAttaSpec.baseMipLevel = depthSpec.baseMipLevel;
    msaaAttaSpec.baseLayer = depthSpec.baseLayer;

    mMsaaDepthAttachment = createAttachment(msaaAttaSpec);
    mMsaaDepthAttachment.view = msaaDepthTexture->getImageView(depthSpec.baseMipLevel, depthSpec.baseLayer, VK_IMAGE_ASPECT_DEPTH_BIT);
}

VulkanRenderTarget::~VulkanRenderTarget() {
    for (int index = 0; index < MAX_SUPPORTED_RENDER_TARGET_COUNT; index++) {
        if (mMsaaColorAttachments[index].texture != mColorAttachments[index].texture) {
            delete mMsaaColorAttachments[index].texture;
        }
    }
    if (mMsaaDepthAttachment.texture != mDepthAttachment.texture) {
        delete mMsaaDepthAttachment.texture;
    }
}

void VulkanRenderTarget::setTargetRectToSurface(VkRect2D* bounds) const {
    const auto& extent = getRenderTargetSize();
    flipVertically(bounds, extent.height);
    clampToFramebuffer(bounds, extent.width, extent.height);
}

void VulkanRenderTarget::setTargetRectToSurface(VkViewport* bounds) const {
    flipVertically(bounds, getRenderTargetSize().height);
}

VkExtent2D VulkanRenderTarget::getRenderTargetSize() const {
    if (mOffscreen) {
        return VkExtent2D {mWidth, mHeight};
    }
    return mContext.currentSwapChain->clientSize;
}

VulkanAttachment VulkanRenderTarget::getColorAttachment(int target) const {
    return (mOffscreen || target > 0) ? mColorAttachments[target] : getSwapChainColorAttachment(mContext);
}

VulkanAttachment VulkanRenderTarget::getMsaaColorAttachment(int target) const {
    return mMsaaColorAttachments[target];
}

VulkanAttachment VulkanRenderTarget::getDepthAttachment() const {
    return mOffscreen ? mDepthAttachment : mContext.currentSwapChain->depthAttachment;
}

VulkanAttachment VulkanRenderTarget::getMsaaDepthAttachment() const {
    return mMsaaDepthAttachment;
}

int VulkanRenderTarget::getColorTargetCount(const VulkanRenderPass& pass) const {
    if (!mOffscreen) {
        return 1;
    }
    int count = 0;
    for (int i = 0; i < MAX_SUPPORTED_RENDER_TARGET_COUNT; i++) {
        if (mColorAttachments[i].format == VK_FORMAT_UNDEFINED) {
            continue;
        }

        if (!(pass.subpassMask & (1 << i)) || pass.currentSubpass == 1) {
            count++;
        }
    }
    return count;
}

// Uniform Buffer
VulkanUniformBuffer::VulkanUniformBuffer(VulkanContext& context, VulkanMemoryPool& memoryPool, uint32_t numBytes, BufferUsage usage) : mContext(context), mMemoryPool(memoryPool), mByteCount(numBytes) {

    VkBufferCreateInfo bufferInfo {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = numBytes,
        .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };
    VmaAllocationCreateInfo allocInfo {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY
    };
    vmaCreateBuffer(mContext.allocator, &bufferInfo, &allocInfo, &mGpuBuffer, &mGpuMemory, nullptr);
}

VulkanUniformBuffer::~VulkanUniformBuffer() {
    vmaDestroyBuffer(mContext.allocator, mGpuBuffer, mGpuMemory);
}

void VulkanUniformBuffer::upload(const void* cpuData, uint32_t byteOffset, uint32_t numBytes) {
    VulkanBufferMemory const* bufferMemory = mMemoryPool.acquireBuffer(numBytes);
    void* mapped;
    vmaMapMemory(mContext.allocator, bufferMemory->memory, &mapped);
    ::memcpy(mapped, cpuData, numBytes);
    vmaUnmapMemory(mContext.allocator, bufferMemory->memory);
    vmaFlushAllocation(mContext.allocator, bufferMemory->memory, 0, numBytes);

    const VkCommandBuffer cmdbuffer = mContext.commandpool->get().cmdbuffer;

    VkBufferCopy region { .size = numBytes };
    vkCmdCopyBuffer(cmdbuffer, bufferMemory->buffer, mGpuBuffer, 1, &region);

    VkBufferMemoryBarrier barrier {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_UNIFORM_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = mGpuBuffer,
        .size = VK_WHOLE_SIZE
    };

    vkCmdPipelineBarrier(cmdbuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
            0, 0, nullptr, 1, &barrier, 0, nullptr);
}

void VulkanUniformBuffer::download(void* cpuData, uint32_t byteOffset, uint32_t numBytes) {
    VR_ASSERT(byteOffset == 0);
    VR_ASSERT(bufferMemory->buffer != nullptr);
    VulkanBufferMemory const* bufferMemory = mMemoryPool.acquireBuffer(numBytes);
    const VkCommandBuffer cmdbuffer = mContext.commandpool->get().cmdbuffer;

    VkBufferCopy region { .size = numBytes };
    vkCmdCopyBuffer(cmdbuffer, mGpuBuffer, bufferMemory->buffer, 1, &region);

    VkBufferMemoryBarrier barrier {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .buffer = bufferMemory->buffer,
        .size = VK_WHOLE_SIZE
    };

    vkCmdPipelineBarrier(cmdbuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT | VK_PIPELINE_STAGE_HOST_BIT, // or VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT
            0, 0, nullptr, 1, &barrier, 0, nullptr);
    
    waitForIdle(mContext);
    
    void* mapped;
    vmaMapMemory(mContext.allocator, bufferMemory->memory, &mapped);
    ::memcpy(cpuData, mapped, numBytes);
    vmaUnmapMemory(mContext.allocator, bufferMemory->memory);
}

// Render Primitive
void VulkanRenderPrimitive::setPrimitiveType(PrimitiveType pt) {
    this->type = pt;
    switch (pt) {
        case PrimitiveType::NONE:
        case PrimitiveType::POINTS:
            primitiveTopology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
            break;
        case PrimitiveType::LINES:
            primitiveTopology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
            break;
        case PrimitiveType::TRIANGLES:
            primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            break;
        case PrimitiveType::TRIANGLE_STRIP:
            primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
            break;
    }
}

void VulkanRenderPrimitive::setBuffers(VulkanVertexBuffer* vertexBuffer, VulkanIndexBuffer* indexBuffer) {
    this->vertexBuffer = vertexBuffer;
    this->indexBuffer = indexBuffer;
}

} // namespace backend
} // namespace VR
