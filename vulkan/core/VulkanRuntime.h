#ifndef VULKAN_RUNTIME_H
#define VULKAN_RUNTIME_H

#include <unordered_map>
#include <vector>

#include "NonCopyable.h"

#include "VulkanContext.h"
#include "VulkanBuffer.h"
#include "VulkanTexture.h"
#include "VulkanProgram.h"
#include "VulkanMemoryPool.h"

#include "VulkanFence.h"
#include "VulkanSemaphore.h"

#include "VulkanRenderTarget.h"
#include "VulkanSwapChain.h"
#include "VulkanSurface.h"

#include "VulkanPipelineCache.h"
#include "VulkanFramebufferCache.h"
#include "VulkanSamplerCache.h"

#include "VulkanUtils.h"

namespace VR {
namespace backend {

struct PipelineState {
    std::shared_ptr<VulkanProgram> program = nullptr;
    RasterStateT rasterState;
    PolygonOffset polygonOffset;
    Viewport scissor{ 0, 0, (uint32_t)std::numeric_limits<int32_t>::max(),(uint32_t)std::numeric_limits<int32_t>::max()};
};

class VulkanRuntime :public NonCopyable {
public:

    VulkanRuntime(VulkanSurface& surface, std::vector<const char *> &ppRequiredExtensions,
                                        std::vector<const char *> &ppRequiredValidationLayers);
    virtual ~VulkanRuntime();

    ShaderModel getShaderModel() const;
    void terminate();
    void update(uint64_t timeStamps);
    void beginFrame(VulkanSwapChain* swapchain, uint64_t timeStamps, uint32_t frameId);
    void endFrame(uint32_t frameId);
    void flush();
    void finish();
    void createUniformBuffer(VulkanUniformBuffer* &uniformBuffer, uint32_t size, BufferUsage usage);
    void destroyUniformBuffer(VulkanUniformBuffer* &uniformBuffer);
    void createRenderPrimitive(VulkanRenderPrimitive* &renderPrimitivet);
    void destroyRenderPrimitive(VulkanRenderPrimitive* &renderPrimitivet);
    void createVertexBuffer(VulkanVertexBuffer* &vertexBuffer, uint8_t bufferCount, uint8_t attributeCount, uint32_t elementCount, AttributeArray attributes);
    void destroyVertexBuffer(VulkanVertexBuffer* &vertexBuffer);
    void createIndexBuffer(VulkanIndexBuffer* &indexBuffer, ElementType elementType, uint32_t indexCount);
    void destroyIndexBuffer(VulkanIndexBuffer* &indexBuffer);
    void createBufferObject(VulkanBufferObject* &bufferObject, uint32_t byteCount);
    void destroyBufferObject(VulkanBufferObject* &bufferObject);
    void createTexture(VulkanTexture* &texture, SamplerType target, uint8_t levels,
                        TextureFormat format, uint8_t samples, uint32_t w, uint32_t h, uint32_t depth, TextureUsage usage);
    void createTextureSwizzled(VulkanTexture* &texture, SamplerType target, uint8_t levels,
                                TextureFormat format, uint8_t samples, uint32_t w, uint32_t h, uint32_t depth,
                                TextureUsage usage, TextureSwizzle r, TextureSwizzle g, TextureSwizzle b, TextureSwizzle a);
    void destroyTexture(VulkanTexture* &texture);
    void createProgram(VulkanProgram* &vkprogram, Program& program, std::string& programName);
    void destroyProgram(VulkanProgram* &vkprogram);
    void createDefaultRenderTarget(VulkanRenderTarget* &renderTarget);
    void createRenderTarget(VulkanRenderTarget* &renderTarget, uint32_t width, uint32_t height, uint8_t samples,
                         VulkanAttachment color[MAX_SUPPORTED_RENDER_TARGET_COUNT], VulkanTexture& depth, VulkanTexture& stencil);
    void destroyRenderTarget(VulkanRenderTarget* &renderTarget);
    void createFence(VulkanFence* &fence);
    void destroyFence(VulkanFence* &fence);
    void createSwapChain(VulkanSwapChain* &swapchain, void* nativeWindow, uint64_t flags);
    void createSwapChainHeadless(VulkanSwapChain* &swapchain, void* nativeWindow, uint32_t width, uint32_t height, uint64_t flags);
    void destroySwapChain(VulkanSwapChain* &swapchain);
    VkResult wait(VulkanFence* fence, uint64_t timeout);
    void setVertexBufferObject(VulkanVertexBuffer* vertexBuffer, uint32_t index, VulkanBufferObject* bufferObject);
    void updateIndexBuffer(VulkanIndexBuffer* indexBuffer, BufferDescriptor& p, uint32_t byteOffset);
    void updateBufferObject(VulkanBufferObject* bufferObject, BufferDescriptor& p, uint32_t byteOffset);
    void update2DImage(VulkanTexture* texture, uint32_t level, uint32_t xoffset, uint32_t yoffset, uint32_t width, uint32_t height,
                 PixelBufferDescriptor& data);
    void setMinMaxLevels(VulkanTexture* texture, uint32_t minLevel, uint32_t maxLevel);
    void update3DImage(VulkanTexture* texture, uint32_t level, uint32_t xoffset, uint32_t yoffset, uint32_t zoffset, uint32_t width, uint32_t height, uint32_t depth, PixelBufferDescriptor& data);
    void updateCubeImage(VulkanTexture* texture, uint32_t level, PixelBufferDescriptor& data, FaceOffsets faceOffsets);
    void loadUniformBuffer(VulkanUniformBuffer* uniformBuffer, BufferDescriptor& data);
    void beginRenderPass(VulkanRenderTarget* renderTarget, const RenderPassParams& params);
    void endRenderPass();
    void nextSubpass();
    void setRenderPrimitiveBuffer(VulkanRenderPrimitive* primitive, VulkanVertexBuffer* vertexBuffer, VulkanIndexBuffer* indexBuffer);
    void setRenderPrimitiveRange(VulkanRenderPrimitive* primitive, PrimitiveType pt, uint32_t offset, uint32_t minIndex, uint32_t maxIndex, uint32_t count);
    void makeCurrent(VulkanSwapChain* drawSch, VulkanSwapChain* readSch);
    void commit(VulkanSwapChain* swapchain);
    void bindUniformBuffer(uint32_t index, VulkanUniformBuffer* uniformBuffer);
    void bindUniformBufferRange(uint32_t index, VulkanUniformBuffer* uniformBuffer, uint32_t offset, uint32_t size);
    void bindSampler(uint32_t index, VulkanSampler& sampler);
    void readPixels(VulkanRenderTarget* renderTarget, uint32_t x, uint32_t y, uint32_t width, uint32_t height, PixelBufferDescriptor& pbd);
    void draw(PipelineState& pipelineState, const VulkanRenderPrimitive* renderPrimitive);
    void createEmptyTexture();
    VulkanContext& getSharedContext() { return mContext; }

private:
    
    void refreshSwapChain();
    void collectGarbage();

    VulkanContext mContext = {};
    VulkanSurface& mSurface;
    VulkanPipelineCache mPipelineCache;
    VulkanMemoryPool mMemoryPool;
    VulkanFramebufferCache mFramebufferCache;
    VulkanSamplerCache mSamplerCache;
    VulkanRenderTarget* mCurrentRenderTarget = nullptr;
    std::vector<VulkanSampler> mSamplerBindings = {};
};

} // namespace backend
} // namespace VR

#endif // VULKAN_RUNTIME_H
