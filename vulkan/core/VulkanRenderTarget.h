#ifndef VULKAN_HANDLES_H
#define VULKAN_HANDLES_H

#include "VulkanContext.h"
#include "VulkanBuffer.h"
#include "VulkanTexture.h"
#include "VulkanSwapChain.h"
#include "VulkanCommandPool.h"
#include "VulkanMemoryPool.h"

namespace VR {
namespace backend {

struct VulkanVertexBuffer : public NonCopyable {
   VulkanVertexBuffer(VulkanContext& context, VulkanMemoryPool& memoryPool, uint8_t bufferCount, uint8_t attributeCount,
                        uint32_t elementCount, AttributeArray const& attribs) : mContext(context), mMemoryPool(memoryPool),
                        bufferCount(bufferCount), attributeCount(attributeCount), vertexCount(elementCount), attributes(attribs), buffers(bufferCount) {}
   
    VulkanContext& mContext;
    VulkanMemoryPool& mMemoryPool;

    AttributeArray attributes{};          
    uint32_t vertexCount{};               
    uint8_t bufferCount{};                
    uint8_t attributeCount{};                  
    std::vector<VulkanBuffer*> buffers;
};

struct VulkanIndexBuffer : public NonCopyable {
    VulkanIndexBuffer(VulkanContext& context, VulkanMemoryPool& memoryPool, uint8_t elementSize, uint32_t indexCount) : mContext(context), mMemoryPool(memoryPool),
                        elementSize(elementSize), indexCount(indexCount), indexType(elementSize == 2 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32),
                        buffer(new VulkanBuffer(context, memoryPool, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, elementSize * indexCount)) {}
    
    VulkanContext& mContext;
    VulkanMemoryPool& mMemoryPool;
    
    uint8_t elementSize{}; // num of bytes
    uint32_t indexCount{};
    uint32_t minIndex{};
    uint32_t maxIndex{};
    const VkIndexType indexType;
    const std::unique_ptr<VulkanBuffer> buffer;
};

struct VulkanBufferObject {
    VulkanBufferObject(VulkanContext& context, VulkanMemoryPool& memoryPool,uint32_t byteCount) : mContext(context), mMemoryPool(memoryPool),
                        byteCount(byteCount), buffer(new VulkanBuffer(context, memoryPool, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, byteCount)) {}
    
    VulkanContext& mContext;
    VulkanMemoryPool& mMemoryPool;
    
    uint32_t byteCount{};
    const std::unique_ptr<VulkanBuffer> buffer;
};

// Uniform Buffer
struct VulkanUniformBuffer : public NonCopyable {
    VulkanUniformBuffer(VulkanContext& context, VulkanMemoryPool& memoryPool, uint32_t numBytes, BufferUsage usage);
    ~VulkanUniformBuffer();

    void upload(const void* cpuData, uint32_t byteOffset, uint32_t numBytes);
    void download(void* cpuData, uint32_t byteOffset, uint32_t numBytes);
    VkBuffer getGpuBuffer() const { return mGpuBuffer; }
    uint32_t getByteCount() const { return mByteCount; }

    VulkanContext& mContext;
    VulkanMemoryPool& mMemoryPool;
    
private:
    uint32_t mByteCount{};
    VkBuffer mGpuBuffer;
    VmaAllocation mGpuMemory;
};

// Render Primitive 
struct VulkanRenderPrimitive : public NonCopyable {
    VulkanRenderPrimitive() {}
    void setPrimitiveType(PrimitiveType pt);
    void setBuffers(VulkanVertexBuffer* vertexBuffer, VulkanIndexBuffer* indexBuffer);
    
    VulkanVertexBuffer* vertexBuffer = nullptr;
    VulkanIndexBuffer* indexBuffer = nullptr;
    VkPrimitiveTopology primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    uint32_t offset{};
    uint32_t minIndex{};
    uint32_t maxIndex{};
    uint32_t count{};
    uint32_t maxVertexCount{};
    PrimitiveType type = PrimitiveType::TRIANGLES;
};

// Redner Target
struct VulkanRenderTarget : public NonCopyable {
    VulkanRenderTarget(VulkanContext& context, VulkanMemoryPool& memoryPool, uint32_t width, uint32_t height, uint8_t samples,
                        VulkanAttachment color[MAX_SUPPORTED_RENDER_TARGET_COUNT], VulkanAttachment depthStencil[2]);

    // default render target
    VulkanRenderTarget(VulkanContext& context, VulkanMemoryPool& memoryPool);
    virtual ~VulkanRenderTarget();

    void setTargetRectToSurface(VkRect2D* bounds) const;
    void setTargetRectToSurface(VkViewport* bounds) const;
    VkExtent2D getRenderTargetSize() const;
    VulkanAttachment getColorAttachment(int target) const;
    VulkanAttachment getMsaaColorAttachment(int target) const;
    VulkanAttachment getDepthAttachment() const;
    VulkanAttachment getMsaaDepthAttachment() const;
    int getColorTargetCount(const VulkanRenderPass& pass) const;
    uint8_t getSamples() const { return mSamples; }
    bool hasDepth() const { return mDepthAttachment.format != VK_FORMAT_UNDEFINED; }
    bool isOffscreen() const { return mOffscreen; }

private:

    VulkanContext& mContext;
    VulkanMemoryPool& mMemoryPool;
    
    uint32_t mWidth{};
    uint32_t mHeight{};
    const uint8_t mSamples = 0;
    const bool mOffscreen;
    
    VulkanAttachment mColorAttachments[MAX_SUPPORTED_RENDER_TARGET_COUNT] = {};
    VulkanAttachment mDepthAttachment = {};
    VulkanAttachment mMsaaColorAttachments[MAX_SUPPORTED_RENDER_TARGET_COUNT] = {};
    VulkanAttachment mMsaaDepthAttachment = {};
};
// helper function
VulkanAttachment createAttachment(VulkanAttachment spec);

} // namespace backend
} // namespace VR

#endif // VULKAN_HANDLES_H
