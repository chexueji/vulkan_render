#include "VulkanRuntime.h"

#ifdef VR_STB_DEBUG
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif

namespace VR {
namespace backend {

VulkanRuntime::VulkanRuntime(VulkanSurface& surface, const std::vector<const char *> &ppRequiredExtensions, const std::vector<const char *> &ppRequiredValidationLayers) :
                            mSurface(surface), mMemoryPool(mContext), mFramebufferCache(mContext), mSamplerCache(mContext) {

    mContext.rasterState = mPipelineCache.getDefaultRasterState();
    // init vulkan functions
    VR_VK_ASSERT(InitVulkan(), "Unable to load vulkan functions.");

    // extensions
    uint32_t instanceExtensionCount;
	CALL_VK(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr));

	std::vector<VkExtensionProperties> instanceExtensions(instanceExtensionCount);
	CALL_VK(vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, instanceExtensions.data()));
            
    const char* ppEnabledExtensions[4];
    uint32_t enabledExtensionCount = 0;

    ppEnabledExtensions[enabledExtensionCount++] = "VK_KHR_surface";
    ppEnabledExtensions[enabledExtensionCount++] = "VK_KHR_get_physical_device_properties2";

    for (uint32_t i = 0; i < ppRequiredExtensions.size(); ++i) {
        ppEnabledExtensions[enabledExtensionCount++] = ppRequiredExtensions[i];
    }
            
#if defined(VR_VULKAN_VALIDATION)
    // validation extension properties
    uint32_t validationExtensionCount;
    CALL_VK(vkEnumerateInstanceExtensionProperties("VK_LAYER_KHRONOS_validation", &validationExtensionCount, nullptr));

    std::vector<VkExtensionProperties> validationExtensions(validationExtensionCount);
    CALL_VK(vkEnumerateInstanceExtensionProperties("VK_LAYER_KHRONOS_validation", &validationExtensionCount, validationExtensions.data()));
            
    ppEnabledExtensions[enabledExtensionCount++] = "VK_EXT_debug_utils";
    ppEnabledExtensions[enabledExtensionCount++] = "VK_EXT_debug_report";
#endif

    // layers
    uint32_t instanceLayerCount;
	CALL_VK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));

	std::vector<VkLayerProperties> supportedValidationLayers(instanceLayerCount);
	CALL_VK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, supportedValidationLayers.data()));

    // vulkan instance
    VkApplicationInfo appInfo = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_MAKE_VERSION(VK_REQUIRED_VERSION_MAJOR, VK_REQUIRED_VERSION_MINOR, 0);

    VkInstanceCreateInfo instanceInfo = {};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = enabledExtensionCount;
    instanceInfo.ppEnabledExtensionNames = ppEnabledExtensions;
    instanceInfo.enabledLayerCount = uint32_t(ppRequiredValidationLayers.size());
	instanceInfo.ppEnabledLayerNames = ppRequiredValidationLayers.data();

    VkResult result = vkCreateInstance(&instanceInfo, VKALLOC, &mContext.instance);
    VR_VK_ASSERT(result == VK_SUCCESS, "Unable to create Vulkan instance.");

    BindVulkan(mContext.instance);

    // physical device
    selectPhysicalDevice(mContext);

    // logical device and graphics queue
    createLogicalDevice(mContext);

    mContext.commandpool = new VulkanCommandPool(mContext.device, mContext.graphicsQueueFamilyIndex);
    // default background
    createEmptyTexture();

    mContext.commandpool->setObserver(&mPipelineCache);
    mPipelineCache.setDevice(mContext.device);

    mContext.depthFormat = findSupportedFormat(mContext, { VK_FORMAT_D32_SFLOAT, VK_FORMAT_X8_D24_UNORM_PACK32 }, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    mSamplerBindings.resize(SAMPLER_BINDING_COUNT);
}

VulkanRuntime::~VulkanRuntime() {
    terminate();
};


ShaderModel VulkanRuntime::getShaderModel() const {
#if defined(ANDROID) || defined(IOS)
    return ShaderModel::GL_ES_30;
#else
    return ShaderModel::GL_CORE_41;
#endif
}

void VulkanRuntime::terminate() {
    if (!mContext.instance) {
        return;
    }

    DELETE_PTR(mContext.commandpool);
    DELETE_PTR(mContext.emptyTexture);

    mMemoryPool.gc();
    mMemoryPool.reset();

    mPipelineCache.destroyCache();
    mFramebufferCache.reset();
    mSamplerCache.reset();

    vmaDestroyAllocator(mContext.allocator);
    vkDestroyCommandPool(mContext.device, mContext.commandPool, VKALLOC);
    vkDestroyDevice(mContext.device, VKALLOC);
    vkDestroyInstance(mContext.instance, VKALLOC);

    mContext.device = nullptr;
    mContext.instance = nullptr;
}

void VulkanRuntime::update(uint64_t timeStamps) {
    mContext.commandpool->updateFences();
}

void VulkanRuntime::collectGarbage() {
    mMemoryPool.gc();
    mFramebufferCache.gc();
    mContext.commandpool->gc();
}

void VulkanRuntime::beginFrame(VulkanSwapChain* swapchain, uint64_t timeStamps, uint32_t frameId) {
    // make the swap chain current
    makeCurrent(swapchain, swapchain);
}

void VulkanRuntime::endFrame(uint32_t frameId) {
    if (mContext.commandpool->flush()) {
        collectGarbage();
    }
}

void VulkanRuntime::flush() {
    mContext.commandpool->flush();
}

void VulkanRuntime::finish() {
    mContext.commandpool->flush();
    // FIXME: should remove wait() 
    mContext.commandpool->wait();
}

void VulkanRuntime::createUniformBuffer(VulkanUniformBuffer* &uniformBuffer, uint32_t size, BufferUsage usage) {
    uniformBuffer = new VulkanUniformBuffer(mContext, mMemoryPool, size, usage);
}

void VulkanRuntime::destroyUniformBuffer(VulkanUniformBuffer* &uniformBuffer) {
    if (uniformBuffer != nullptr) {
        mPipelineCache.unbindUniformBuffer(uniformBuffer->getGpuBuffer());
        DELETE_PTR(uniformBuffer);
    }
}

void VulkanRuntime::createRenderPrimitive(VulkanRenderPrimitive* &renderPrimitivet) {
    renderPrimitivet = new VulkanRenderPrimitive();
}

void VulkanRuntime::destroyRenderPrimitive(VulkanRenderPrimitive* &renderPrimitivet) {
    DELETE_PTR(renderPrimitivet);
}

void VulkanRuntime::createVertexBuffer(VulkanVertexBuffer* &vertexBuffer, uint8_t bufferCount, uint8_t attributeCount,
                                       uint32_t elementCount, AttributeArray attributes) {
    vertexBuffer = new VulkanVertexBuffer(mContext, mMemoryPool, bufferCount, attributeCount, elementCount, attributes);
}

void VulkanRuntime::destroyVertexBuffer(VulkanVertexBuffer* &vertexBuffer) {
    DELETE_PTR(vertexBuffer);
}

void VulkanRuntime::createIndexBuffer(VulkanIndexBuffer* &indexBuffer, ElementType elementType, uint32_t indexCount) {
    auto elementSize = (uint8_t) getElementTypeSize(elementType);
    indexBuffer = new VulkanIndexBuffer(mContext, mMemoryPool, elementSize, indexCount);
}

void VulkanRuntime::destroyIndexBuffer(VulkanIndexBuffer* &indexBuffer) {
    DELETE_PTR(indexBuffer);
}

void VulkanRuntime::createBufferObject(VulkanBufferObject* &bufferObject, uint32_t byteCount) {
    bufferObject = new VulkanBufferObject(mContext, mMemoryPool, byteCount);
}

void VulkanRuntime::destroyBufferObject(VulkanBufferObject* &bufferObject) {
    DELETE_PTR(bufferObject);
}

void VulkanRuntime::createTexture(VulkanTexture* &texture, SamplerType target, uint8_t levels,
                                  TextureFormat format, uint8_t samples, uint32_t w, uint32_t h, uint32_t depth, TextureUsage usage) {
    texture = new VulkanTexture(mContext, target, levels, format, samples, w, h, depth, usage, mMemoryPool);
}

void VulkanRuntime::createTextureSwizzled(VulkanTexture* &texture, SamplerType target, uint8_t levels,
                                          TextureFormat format, uint8_t samples, uint32_t w, uint32_t h, uint32_t depth,
                                          TextureUsage usage, TextureSwizzle r, TextureSwizzle g, TextureSwizzle b, TextureSwizzle a) {
    TextureSwizzle swizzleArray[] = {r, g, b, a};
    const VkComponentMapping swizzleMap = getSwizzleMap(swizzleArray);
    texture = new VulkanTexture(mContext, target, levels, format, samples, w, h, depth, usage, mMemoryPool, swizzleMap);
}

void VulkanRuntime::destroyTexture(VulkanTexture* &texture) {
    if (texture != nullptr) {
        mPipelineCache.unbindImageView(texture->getPrimaryImageView());
        DELETE_PTR(texture);
    }
}

void VulkanRuntime::createProgram(VulkanProgram* &vkprogram, Program& program, std::string& programName) {
    vkprogram = new VulkanProgram(mContext, program, programName);
}

void VulkanRuntime::destroyProgram(VulkanProgram* &vkprogram) {
    DELETE_PTR(vkprogram);
}

void VulkanRuntime::createDefaultRenderTarget(VulkanRenderTarget* &renderTarget) {
    renderTarget = new VulkanRenderTarget(mContext, mMemoryPool);
}

void VulkanRuntime::createRenderTarget(VulkanRenderTarget* &renderTarget, uint32_t width, uint32_t height, uint8_t samples,
                                        VulkanAttachment color[MAX_SUPPORTED_RENDER_TARGET_COUNT], VulkanTexture& depth, VulkanTexture& stencil) {

    VulkanAttachment colorTargets[MAX_SUPPORTED_RENDER_TARGET_COUNT] = {};
    for (int i = 0; i < MAX_SUPPORTED_RENDER_TARGET_COUNT; i++) {
        colorTargets[i].texture = color[i].texture;
        colorTargets[i].baseMipLevel = color[i].baseMipLevel;
        colorTargets[i].baseLayer = color[i].baseLayer;
    }

    VulkanAttachment depthStencil[2] = {};
    depthStencil[0].texture = &depth;
    depthStencil[0].baseMipLevel = 0u;
    depthStencil[0].baseLayer = 0u;

    depthStencil[1].texture = &stencil;
    depthStencil[1].baseMipLevel = 0u;
    depthStencil[1].baseLayer = 0u;

    renderTarget = new VulkanRenderTarget(mContext, mMemoryPool, width, height, samples, colorTargets, depthStencil);
}

void VulkanRuntime::destroyRenderTarget(VulkanRenderTarget* &renderTarget) {
    DELETE_PTR(renderTarget);
}

void VulkanRuntime::createFence(VulkanFence* &fence) {
    fence = new VulkanFence(mContext.device);
}

void VulkanRuntime::createSwapChain(VulkanSwapChain* &swapchain, void* nativeWindow, uint64_t flags) {
    auto vksurface = (VkSurfaceKHR) mSurface.createVkSurfaceKHR(nativeWindow, mContext.instance, flags);
    swapchain = new VulkanSwapChain(mContext, vksurface);
}

void VulkanRuntime::createSwapChainHeadless(VulkanSwapChain* &swapchain, void* nativeWindow, uint32_t width, uint32_t height, uint64_t flags) {
    auto vksurface = (VkSurfaceKHR) mSurface.createVkSurfaceKHR(nativeWindow, mContext.instance, flags);
    swapchain = new VulkanSwapChain(mContext, width, height);
}

void VulkanRuntime::destroySwapChain(VulkanSwapChain* &swapchain) {
    if (swapchain) {  
        swapchain->destroy();
        vkDestroySurfaceKHR(mContext.instance, swapchain->surface, VKALLOC);
        if (mContext.currentSwapChain == swapchain) {
            mContext.currentSwapChain = nullptr;
        }
    }
}

void VulkanRuntime::destroyFence(VulkanFence* &fence) {
    DELETE_PTR(fence);
}

VkResult VulkanRuntime::wait(VulkanFence* fence, uint64_t timeout) {
    VR_ASSERT(fence != nullptr)
    const auto& cmdfence = fence->get();
    VkResult result = vkWaitForFences(mContext.device, 1, &cmdfence, VK_TRUE, timeout);
    return result;
}

void VulkanRuntime::setVertexBufferObject(VulkanVertexBuffer* vertexBuffer, uint32_t index, VulkanBufferObject* bufferObject) {
    VR_ASSERT(vertexBuffer != nullptr);
    vertexBuffer->buffers[index] = bufferObject->buffer.get();
}

void VulkanRuntime::updateIndexBuffer(VulkanIndexBuffer* indexBuffer, BufferDescriptor& p, uint32_t byteOffset) {
    VR_ASSERT(indexBuffer != nullptr);
    indexBuffer->buffer->upload(p.buffer, byteOffset, p.size);
}

void VulkanRuntime::updateBufferObject(VulkanBufferObject* bufferObject, BufferDescriptor& p, uint32_t byteOffset) {
    VR_ASSERT(bufferObject != nullptr);
    bufferObject->buffer->upload(p.buffer, byteOffset, p.size);
}

void VulkanRuntime::update2DImage(VulkanTexture* texture, uint32_t level, uint32_t xoffset, uint32_t yoffset, uint32_t width, uint32_t height,
                                PixelBufferDescriptor& data) {
    VR_ASSERT(texture != nullptr);
    texture->update2DImage(data, width, height, level);
}

void VulkanRuntime::setMinMaxLevels(VulkanTexture* texture, uint32_t minLevel, uint32_t maxLevel) {
    VR_ASSERT(texture != nullptr);
    texture->setPrimaryRange(minLevel, maxLevel);
}

void VulkanRuntime::update3DImage(VulkanTexture* texture, uint32_t level, uint32_t xoffset, uint32_t yoffset, uint32_t zoffset,
                                  uint32_t width, uint32_t height, uint32_t depth, PixelBufferDescriptor& data) {
    VR_ASSERT(texture != nullptr);
    texture->update3DImage(data, width, height, depth, level);
}

void VulkanRuntime::updateCubeImage(VulkanTexture* texture, uint32_t level, PixelBufferDescriptor& data, FaceOffsets faceOffsets) {
    VR_ASSERT(texture != nullptr);
    texture->updateCubeImage(data, faceOffsets, level);
}

void VulkanRuntime::loadUniformBuffer(VulkanUniformBuffer* uniformBuffer, BufferDescriptor& data) {
    VR_ASSERT(uniformBuffer != nullptr);
    uniformBuffer->upload(data.buffer, 0, (uint32_t)data.size);
}

void VulkanRuntime::beginRenderPass(VulkanRenderTarget* renderTarget, const RenderPassParams& params) {
    if (renderTarget == nullptr) {
        VR_ERROR("No render target.");
        return;
    }
    
    uint32_t curSwapchainIndex = getSwapChainIndex(mContext);
    mCurrentRenderTarget = renderTarget;

    const VkExtent2D extent = renderTarget->getRenderTargetSize();
    VR_ASSERT(extent.width > 0 && extent.height > 0);

    TargetBufferFlags discardStart = params.flags.discardStart;

    const VkCommandBuffer cmdbuffer = mContext.commandpool->get().cmdbuffer;
    VulkanAttachment depth = renderTarget->getDepthAttachment();
    VulkanTexture* depthFeedback = nullptr;

//    if (depth.texture && any(params.flags.discardEnd & TargetBufferFlags::DEPTH) && !any(params.flags.clear & TargetBufferFlags::DEPTH)) {
//        depthFeedback = depth.texture;
//        const VulkanLayoutTransition transition = {
//         .image = depth.image,
//         .oldLayout = depth.layout,
//         .newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
//         .subresources = {
//             .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
//             .levelCount = depth.texture->getMipLevels(),
//             .layerCount = depth.texture->getDepth(),
//         },
//         .srcStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
//         .dstStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
//        };
//        transitionImageLayout(cmdbuffer, transition);
//        depth.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
//    }

    VulkanFramebufferCache::RenderPassInfo rpInfo = {
        .depthLayout = depth.layout,
        .depthFormat = depth.format,
        .clear = params.flags.clear,
        .discardStart = discardStart,
        .discardEnd = params.flags.discardEnd,
        .samples = renderTarget->getSamples(),
        .subpassMask = uint8_t(params.subpassMask)
    };
    
    for (int i = 0; i < MAX_SUPPORTED_RENDER_TARGET_COUNT; i++) {
        rpInfo.colorLayout[i] = renderTarget->getColorAttachment(i).layout;
        rpInfo.colorFormat[i] = renderTarget->getColorAttachment(i).format;
        VulkanTexture* texture = renderTarget->getColorAttachment(i).texture;
        if (rpInfo.samples > 1 && texture && texture->samples() == 1) {
            rpInfo.needsResolveMask |= (1 << i);
        }
    }
    
    // get renderpass object
    VkRenderPass renderPass = mFramebufferCache.getRenderPass(rpInfo, curSwapchainIndex);
    mPipelineCache.bindRenderPass(renderPass, 0);

    VulkanFramebufferCache::FrameBufferInfo fbInfo {
        .renderPass = renderPass,
        .width = (uint16_t) extent.width,
        .height = (uint16_t) extent.height,
        .layers = 1,
        .samples = rpInfo.samples
    };
    
    // color attachment
    for (int i = 0; i < MAX_SUPPORTED_RENDER_TARGET_COUNT; i++) {
        if (renderTarget->getColorAttachment(i).format == VK_FORMAT_UNDEFINED) {
            // no render target, for render test purpose
            fbInfo.color[i] = VK_NULL_HANDLE;
            fbInfo.resolve[i] = VK_NULL_HANDLE;
        } else if (fbInfo.samples == 1) {
            // bind the image view of render target to current framebuffer
            fbInfo.color[i] = renderTarget->getColorAttachment(i).view;
            fbInfo.resolve[i] = VK_NULL_HANDLE;
            VR_ASSERT(fbInfo.color[i]);
        } else {
            // bind the image view of render target to current framebuffer, for msaa
            fbInfo.color[i] = renderTarget->getMsaaColorAttachment(i).view;
            VulkanTexture* texture = renderTarget->getColorAttachment(i).texture;
            if (texture && texture->samples() == 1) {
                fbInfo.resolve[i] = renderTarget->getColorAttachment(i).view;
                VR_ASSERT(fbInfo.resolve[i]);
            }
            VR_ASSERT(fbInfo.color[i]);
        }
    }
    // depth attachment
    if (depth.format != VK_FORMAT_UNDEFINED) {
        fbInfo.depth = rpInfo.samples == 1 ? depth.view : renderTarget->getMsaaDepthAttachment().view;
        VR_ASSERT(fbInfo.depth);
    }
    
    // get framebuffer object
    VkFramebuffer vkFramebuffer = mFramebufferCache.getFramebuffer(fbInfo, curSwapchainIndex);
    
    // set begininfo of render pass
    VkRenderPassBeginInfo renderPassInfo {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = vkFramebuffer,
        .renderArea = { .offset = {}, .extent = extent }
    };

    renderTarget->setTargetRectToSurface(&renderPassInfo.renderArea);

    VkClearValue clearValues[MAX_SUPPORTED_RENDER_TARGET_COUNT + MAX_SUPPORTED_RENDER_TARGET_COUNT + 1] = {};

    for (int i = 0; i < MAX_SUPPORTED_RENDER_TARGET_COUNT; i++) {
        if (fbInfo.color[i]) {
            VkClearValue& clearValue = clearValues[renderPassInfo.clearValueCount++];
            clearValue.color.float32[0] = params.clearColor[0];
            clearValue.color.float32[1] = params.clearColor[1];
            clearValue.color.float32[2] = params.clearColor[2];
            clearValue.color.float32[3] = params.clearColor[3];
        }
    }

    for (int i = 0; i < MAX_SUPPORTED_RENDER_TARGET_COUNT; i++) {
        if (rpInfo.needsResolveMask & (1u << i)) {
            renderPassInfo.clearValueCount++;
        }
    }
    
    // set clear value of depth stencil
    if (fbInfo.depth) {
        VkClearValue& clearValue = clearValues[renderPassInfo.clearValueCount++];
        clearValue.depthStencil = {(float) params.clearDepth, 0};
    }
    renderPassInfo.pClearValues = &clearValues[0];
    // begin render pass
    vkCmdBeginRenderPass(cmdbuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport = mContext.viewport = {
        .x = (float) params.viewport.left,
        .y = (float) params.viewport.bottom,
        .width = (float) params.viewport.width,
        .height = (float) params.viewport.height,
        .minDepth = params.depthRange.near,
        .maxDepth = params.depthRange.far
    };

    mCurrentRenderTarget->setTargetRectToSurface(&viewport);
    vkCmdSetViewport(cmdbuffer, 0, 1, &viewport);

    mContext.currentRenderPass = {
        .renderPass = renderPassInfo.renderPass,
        .subpassMask = params.subpassMask,
        .currentSubpass = 0,
        .depthFeedback = depthFeedback
    };
}

void VulkanRuntime::endRenderPass() {
    VkCommandBuffer cmdbuffer = mContext.commandpool->get().cmdbuffer;
    vkCmdEndRenderPass(cmdbuffer);

    VR_ASSERT(mCurrentRenderTarget);

    VulkanTexture* depthFeedback = mContext.currentRenderPass.depthFeedback;
    if (depthFeedback) {
        const VulkanLayoutTransition transition = {
            .image = depthFeedback->vkImage(),
            .oldLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .subresources = {
                .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
                .levelCount = depthFeedback->mipLevels(),
                .layerCount = depthFeedback->depth(),
            },
            .srcStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            .dstStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
        };
        transitionImageLayout(cmdbuffer, transition);
    }

    // add barrier to read content from framebuffer
    if (mCurrentRenderTarget->isOffscreen()) {
        VkMemoryBarrier barrier {
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        };
        VkPipelineStageFlags srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        if (mCurrentRenderTarget->hasDepth()) {
            barrier.srcAccessMask |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            srcStageMask |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        }
        vkCmdPipelineBarrier(cmdbuffer, srcStageMask,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT | 
                VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                0, 1, &barrier, 0, nullptr, 0, nullptr);
    }
    // reset render target and render pass
    mCurrentRenderTarget = VK_NULL_HANDLE;
    if (mContext.currentRenderPass.currentSubpass > 0) {
        for (uint32_t i = 0; i < TARGET_BINDING_COUNT; i++) {
            mPipelineCache.bindInputAttachment(i, {});
        }
        mContext.currentRenderPass.currentSubpass = 0;
    }
    mContext.currentRenderPass.renderPass = VK_NULL_HANDLE;
}

void VulkanRuntime::nextSubpass() {
    VR_VK_ASSERT(mContext.currentRenderPass.currentSubpass == 0, "2 subpasses  supported");
    VR_ASSERT(mContext.currentSwapChain);
    VR_ASSERT(mCurrentRenderTarget);
    VR_ASSERT(mContext.currentRenderPass.subpassMask);

    // use the same command buffer for next subpass
    vkCmdNextSubpass(mContext.commandpool->get().cmdbuffer, VK_SUBPASS_CONTENTS_INLINE);

    mPipelineCache.bindRenderPass(mContext.currentRenderPass.renderPass, ++mContext.currentRenderPass.currentSubpass);

    for (uint32_t i = 0; i < TARGET_BINDING_COUNT; i++) {
        if ((1 << i) & mContext.currentRenderPass.subpassMask) {
            VulkanAttachment subpassInput = mCurrentRenderTarget->getColorAttachment(i);
            VkDescriptorImageInfo info = {
                .imageView = subpassInput.view,
                .imageLayout = subpassInput.layout,
            };
            mPipelineCache.bindInputAttachment(i, info);
        }
    }
}

void VulkanRuntime::setRenderPrimitiveBuffer(VulkanRenderPrimitive* primitive, VulkanVertexBuffer* vertexBuffer, VulkanIndexBuffer* indexBuffer) {
    primitive->setBuffers(vertexBuffer, indexBuffer);
}

void VulkanRuntime::setRenderPrimitiveRange(VulkanRenderPrimitive* primitive, PrimitiveType pt, uint32_t offset, uint32_t minIndex, uint32_t maxIndex, uint32_t count) {
    primitive->setPrimitiveType(pt);
    primitive->offset = offset * primitive->indexBuffer->elementSize;
    primitive->count = count;
    primitive->minIndex = minIndex;
    primitive->maxIndex = maxIndex > minIndex ? maxIndex : primitive->maxVertexCount - 1;
}

void VulkanRuntime::makeCurrent(VulkanSwapChain* drawSch, VulkanSwapChain* readSch) {
    VR_ASSERT(drawSch == readSch && drawSch != nullptr);
    mContext.currentSwapChain = drawSch;

    // acquired but not yet presented
    if (drawSch->acquired) {
        return;
    }

    if (drawSch->hasResized()) {
        refreshSwapChain();
    }
    // acquire next drawable image
    drawSch->acquireNextImage();
}

void VulkanRuntime::commit(VulkanSwapChain* swapchain) {
    VR_VK_ASSERT(swapchain != nullptr, "No swap chain.");
    // swap chain image to present layout
    swapchain->makePresentable();

    if (mContext.commandpool->flush()) {
        collectGarbage();
    }

    if (swapchain->headlessQueue) {
        return;
    }

    swapchain->acquired = false;
    // wait for render finished signal, then present image
    VkSemaphore renderingFinished = mContext.commandpool->getRenderFinishedSignal();
    VkPresentInfoKHR presentInfo {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &renderingFinished,
        .swapchainCount = 1,
        .pSwapchains = &swapchain->swapchain,
        .pImageIndices = &swapchain->currentSwapIndex,
    };
    VkResult result = vkQueuePresentKHR(swapchain->presentQueue, &presentInfo);

    if (result == VK_SUBOPTIMAL_KHR && !swapchain->suboptimal) {
        swapchain->suboptimal = true;
    }
    // handle outdated error in present,e.g. swap chain resize
    if (result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        // TODO: resize
    }
    // swap chain resize with VK_ERROR_OUT_OF_DATE_KHR
    VR_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR);
}

void VulkanRuntime::bindUniformBuffer(uint32_t index, VulkanUniformBuffer* uniformBuffer) {
    VR_VK_ASSERT(uniformBuffer != nullptr, "No uniform buffer.");
    const VkDeviceSize offset = 0;
    const VkDeviceSize size = VK_WHOLE_SIZE;
    mPipelineCache.bindUniformBuffer((uint32_t) index, uniformBuffer->getGpuBuffer(), offset, size);
}

void VulkanRuntime::bindUniformBufferRange(uint32_t index, VulkanUniformBuffer* uniformBuffer, uint32_t offset, uint32_t size) {
    VR_VK_ASSERT(uniformBuffer != nullptr, "No uniform buffer.");
    mPipelineCache.bindUniformBuffer((uint32_t)index, uniformBuffer->getGpuBuffer(), offset, size);
}

void VulkanRuntime::bindSampler(uint32_t index, VulkanSampler& sampler) {
    mSamplerBindings[index] = std::move(sampler);
}

void VulkanRuntime::readPixels(VulkanRenderTarget* renderTarget, uint32_t x, uint32_t y, uint32_t width, uint32_t height, PixelBufferDescriptor& pbd) {
    VR_VK_ASSERT(renderTarget != nullptr, "No render target.");
    const VkDevice device = mContext.device;
    const VulkanTexture* srcTexture = renderTarget->getColorAttachment(0).texture;
    const VkFormat targetTextureFormat = srcTexture ? srcTexture->vkFormat() : mContext.currentSwapChain->surfaceFormat.format;
    const bool swizzle = targetTextureFormat == VK_FORMAT_B8G8R8A8_UNORM;

    VkImageCreateInfo imageInfo {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = targetTextureFormat,
        .extent = { width, height, 1 },
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_LINEAR,
        .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
    };

    VkImage image;
    vkCreateImage(device, &imageInfo, VKALLOC, &image);

    VkMemoryRequirements memReqs;
    VkDeviceMemory deviceMemory;
    vkGetImageMemoryRequirements(device, image, &memReqs);
    VkMemoryAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = memReqs.size,
        .memoryTypeIndex = selectMemoryType(mContext, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)
    };

    vkAllocateMemory(device, &allocInfo, nullptr, &deviceMemory);
    vkBindImageMemory(device, image, deviceMemory, 0);

    // image barrier
    waitForIdle(mContext);

    // image layout
    const VkCommandBuffer cmdbuffer = mContext.commandpool->get().cmdbuffer;

    transitionImageLayout(cmdbuffer, {
        .image = image,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .subresources = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
        .srcStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .srcAccessMask = 0,
        .dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    });

    const VulkanAttachment attach = renderTarget->getColorAttachment(0);

    VkImageCopy imageCopyRegion = {
        .srcSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .mipLevel = attach.baseMipLevel,
            .baseArrayLayer = attach.baseLayer,
            .layerCount = 1,
        },
        .srcOffset = {
            .x = (int32_t) x,
            .y = (int32_t) y,
        },
        .dstSubresource = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .layerCount = 1,
        },
        .extent = {
            .width = width,
            .height = height,
            .depth = 1,
        },
    };

    // image layout to other layout
    const VkImageSubresourceRange srcRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = attach.baseMipLevel,
        .levelCount = 1,
        .baseArrayLayer = attach.baseLayer,
        .layerCount = 1,
    };

    VkImage srcImage = renderTarget->getColorAttachment(0).image;
    transitionImageLayout(cmdbuffer, {
        .image = srcImage,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        .subresources = srcRange,
        .srcStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        .srcAccessMask = 0,
        .dstStage = VK_PIPELINE_STAGE_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT,
    });

    // blit image
    vkCmdCopyImage(cmdbuffer, renderTarget->getColorAttachment(0).image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &imageCopyRegion);

    // save the src image layout
    if (srcTexture || mContext.currentSwapChain->presentQueue) {
        const VkImageLayout present = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        transitionImageLayout(cmdbuffer, {
            .image = srcImage,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, // FIXME: srcTexture ? getTextureLayout(srcTexture->usage()) : present,
            .subresources = srcRange,
            .srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        });
    } else {
        transitionImageLayout(cmdbuffer, {
            .image = srcImage,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .subresources = srcRange,
            .srcStage = VK_PIPELINE_STAGE_TRANSFER_BIT,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        });
    }

    // image layout to GENERAL
    VkImageMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_GENERAL,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    vkCmdPipelineBarrier(cmdbuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

    mContext.commandpool->flush();
    mContext.commandpool->wait();

    VkImageSubresource subResource { .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT };
    VkSubresourceLayout subResourceLayout;
    vkGetImageSubresourceLayout(device, image, &subResource, &subResourceLayout);

    // map image memory
    const uint8_t* srcPixels;
    vkMapMemory(device, deviceMemory, 0, VK_WHOLE_SIZE, 0, (void**) &srcPixels);
    srcPixels += subResourceLayout.offset;
#ifdef VR_STB_DEBUG
    // dump screen content
    stbi_write_png("./screendump.png", width,  height, 4, srcPixels, 4 * width);
#endif
    vkUnmapMemory(device, deviceMemory);
    vkFreeMemory(device, deviceMemory, nullptr);
    vkDestroyImage(device, image, nullptr);
}

void VulkanRuntime::draw(PipelineState& pipelineState, const VulkanRenderPrimitive* renderPrimitive) {
    VR_VK_ASSERT(renderPrimitive != nullptr, "No render primitive.");

    VulkanCommandBuffer const* commandpool = &mContext.commandpool->get();
    VkCommandBuffer cmdbuffer = commandpool->cmdbuffer;

    std::shared_ptr<VulkanProgram> program = pipelineState.program;
    RasterStateT& rasterState = pipelineState.rasterState;
    PolygonOffset& depthOffset = pipelineState.polygonOffset;
    const Viewport& viewportScissor = pipelineState.scissor;

    const VulkanRenderTarget* rt = mCurrentRenderTarget;
    mContext.rasterState.depthStencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = (VkBool32) rasterState.depthWrite,
        .depthCompareOp = getCompareOp(rasterState.depthFunc),
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
    };

    mContext.rasterState.multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = (VkSampleCountFlagBits) rt->getSamples(),
        .alphaToCoverageEnable = rasterState.alphaToCoverage,
    };

    mContext.rasterState.blending = {
        .blendEnable = (VkBool32) rasterState.hasBlending(),
        .srcColorBlendFactor = getBlendFactor(rasterState.blendFunctionSrcRGB),
        .dstColorBlendFactor = getBlendFactor(rasterState.blendFunctionDstRGB),
        .colorBlendOp = (VkBlendOp) rasterState.blendEquationRGB,
        .srcAlphaBlendFactor = getBlendFactor(rasterState.blendFunctionSrcAlpha),
        .dstAlphaBlendFactor = getBlendFactor(rasterState.blendFunctionDstAlpha),
        .alphaBlendOp =  (VkBlendOp) rasterState.blendEquationAlpha,
        .colorWriteMask = (VkColorComponentFlags) (rasterState.colorWrite ? 0xf : 0x0),
    };

    VkPipelineRasterizationStateCreateInfo& vkraster = mContext.rasterState.rasterization;
    vkraster.cullMode = getCullMode(rasterState.culling);
    vkraster.frontFace = getFrontFace(rasterState.inverseFrontFaces);
    vkraster.depthBiasEnable = (depthOffset.constant || depthOffset.slope) ? VK_TRUE : VK_FALSE;
    vkraster.depthBiasConstantFactor = depthOffset.constant;
    vkraster.depthBiasSlopeFactor = depthOffset.slope;

    mContext.rasterState.colorTargetCount = rt->getColorTargetCount(mContext.currentRenderPass);

    VulkanPipelineCache::VertexAttributeArray varray = {};
    VkBuffer buffers[MAX_VERTEX_ATTRIBUTE_COUNT] = {};
    VkDeviceSize offsets[MAX_VERTEX_ATTRIBUTE_COUNT] = {};

    // process each attribute
    const uint32_t bufferCount = renderPrimitive->vertexBuffer->attributes.size();
    uint32_t realBufferCount = 0;
    for (uint32_t attribIndex = 0; attribIndex < bufferCount; attribIndex++) {
        Attribute attrib = renderPrimitive->vertexBuffer->attributes[attribIndex];

        const bool isInteger = attrib.flags & Attribute::FLAG_INTEGER_TARGET;
        const bool isNormalized = attrib.flags & Attribute::FLAG_NORMALIZED;

        VkFormat vkformat = getVkFormat(attrib.type, isNormalized, isInteger);

        if (attrib.buffer == Attribute::BUFFER_UNUSED) {
            vkformat = isInteger ? VK_FORMAT_R8G8B8A8_UINT : VK_FORMAT_R8G8B8A8_SNORM;
            attrib = renderPrimitive->vertexBuffer->attributes[0];
            //FIXME: should pass
            continue;
        } else {
            realBufferCount++;
        }
        // FIXME:attrib.buffer should be -1, if attribute num is larger than 16
        const VulkanBuffer* buffer = renderPrimitive->vertexBuffer->buffers[attrib.buffer];

        if (buffer == nullptr) {
            return;
        }

        buffers[attribIndex] = buffer->getGpuBuffer();
        offsets[attribIndex] = attrib.offset;
        varray.attributes[attribIndex] = {
            .location = attribIndex, // GLSL layout specifier
            .binding = attribIndex, // FIXME: should be buffer index
            .format = vkformat,
        };
        varray.buffers[attribIndex] = {
            .binding = attribIndex,
            .stride = attrib.stride,
        };
    }

    const std::vector<VkShaderModule>& shaderModules = program->getShaderModules();
    mPipelineCache.bindProgram(shaderModules[0], shaderModules[1]);
    mPipelineCache.bindRasterState(mContext.rasterState);
    mPipelineCache.bindPrimitiveTopology(renderPrimitive->primitiveTopology);
    mPipelineCache.bindVertexAttributeArray(varray);

    VkDescriptorImageInfo samplers[SAMPLER_BINDING_COUNT] = {};
    // set vulkan samplers
    for (uint8_t samplerIdx = 0; samplerIdx < SAMPLER_BINDING_COUNT; samplerIdx++) {
        const auto& sampler = program->getSamplerBlockInfo(samplerIdx);
        if (sampler.name.empty()) {
            continue;
        }
        
        VulkanSampler& boundSampler = mSamplerBindings[samplerIdx];
        if (boundSampler.texture == nullptr) {
            continue;
        }

        size_t bindingPoint = sampler.binding;
        
        const VulkanTexture* texture;
        if (boundSampler.texture->vkImage() == VK_NULL_HANDLE) {
            texture = mContext.emptyTexture;
        } else {
            texture = boundSampler.texture.get();
        }

        const SamplerParams& samplerParams = boundSampler.samplerParam;
        VkSampler vksampler = mSamplerCache.getSampler(samplerParams);

        samplers[bindingPoint] = {
            .sampler = vksampler,
            .imageView = texture->getPrimaryImageView(),
            .imageLayout = getTextureLayout(texture->usage())
        };

        if (mContext.currentRenderPass.depthFeedback == texture) {
            samplers[bindingPoint].imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
        }
        
    }

    mPipelineCache.bindSamplers(samplers);

    if (!mPipelineCache.bindDescriptorSets(cmdbuffer)) {
        return;
    }

    // intersection of viewports
    const int32_t x = std::max(viewportScissor.left, (int32_t)mContext.viewport.x);
    const int32_t y = std::max(viewportScissor.bottom, (int32_t)mContext.viewport.y);
    const int32_t right = std::min(viewportScissor.left + (int32_t)viewportScissor.width, (int32_t)(mContext.viewport.x + mContext.viewport.width));
    const int32_t top = std::min(viewportScissor.bottom + (int32_t)viewportScissor.height, (int32_t)(mContext.viewport.y + mContext.viewport.height));
    VkRect2D scissor{ .offset = { std::max(0, x), std::max(0, y) }, .extent = { (uint32_t)right - x, (uint32_t)top - y }};

    rt->setTargetRectToSurface(&scissor);
    mPipelineCache.bindScissor(cmdbuffer, scissor);
    mPipelineCache.bindPipeline(cmdbuffer);

    // bind the vertex buffers and index buffer
    // FIXME: use realBufferCount or bufferCount
    vkCmdBindVertexBuffers(cmdbuffer, 0, realBufferCount, buffers, offsets);
    vkCmdBindIndexBuffer(cmdbuffer, renderPrimitive->indexBuffer->buffer->getGpuBuffer(), 0, renderPrimitive->indexBuffer->indexType);

    const uint32_t indexCount = renderPrimitive->count;
    const uint32_t instanceCount = 1;
    const uint32_t firstIndex = renderPrimitive->offset / renderPrimitive->indexBuffer->elementSize;
    const int32_t vertexOffset = 0;
    const uint32_t firstInstId = 1;
    vkCmdDrawIndexed(cmdbuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstId);
}

void VulkanRuntime::refreshSwapChain() {
    VulkanSwapChain& surface = *mContext.currentSwapChain;

    VR_VK_ASSERT(!surface.headlessQueue, "Resizing headless swap chains is not supported.");
    surface.destroy();
    surface.create();

    mFramebufferCache.reset();
}

void VulkanRuntime::createEmptyTexture() {
    mContext.emptyTexture = new VulkanTexture(mContext, SamplerType::SAMPLER_2D, 1, TextureFormat::RGBA8, 1, 1, 1, 1, TextureUsage::DEFAULT | TextureUsage::COLOR_ATTACHMENT |
            TextureUsage::SUBPASS_INPUT, mMemoryPool);
    uint32_t black = 0;
    PixelBufferDescriptor pixelBufferDes(&black, 4, PixelDataFormat::RGBA, PixelDataType::UBYTE);
    mContext.emptyTexture->update2DImage(pixelBufferDes, 1, 1, 0);
}

} // namespace backend
} // namespace VR
