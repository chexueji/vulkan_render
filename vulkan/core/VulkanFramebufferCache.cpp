#include "VulkanMacros.h"
#include "VulkanFramebufferCache.h"

namespace VR {
namespace backend {

VulkanFramebufferCache::VulkanFramebufferCache(VulkanContext& context) : mContext(context) {
    mRenderPasses.resize(VK_MAX_COMMAND_BUFFERS);
    mFramebuffers.resize(VK_MAX_COMMAND_BUFFERS);
}

VulkanFramebufferCache::~VulkanFramebufferCache() {
    // Do nothing
}

VkFramebuffer VulkanFramebufferCache::getFramebuffer(const FrameBufferInfo& fboInfo, uint32_t swapchainIndex) {
    if (mFramebuffers.size() < VK_MAX_COMMAND_BUFFERS) {
        mFramebuffers.resize(VK_MAX_COMMAND_BUFFERS);
    }
    
    if (mFramebuffers[swapchainIndex] != VK_NULL_HANDLE) {
        return mFramebuffers[swapchainIndex];
    }
    // color attachments, resolve attachments, and depth attachment
    VkImageView attachments[MAX_SUPPORTED_RENDER_TARGET_COUNT + MAX_SUPPORTED_RENDER_TARGET_COUNT + 1];
    uint32_t attachmentCount = 0;
    for (VkImageView attachment : fboInfo.color) {
        if (attachment) {
            attachments[attachmentCount++] = attachment;
        }
    }
    for (VkImageView attachment : fboInfo.resolve) {
        if (attachment) {
            attachments[attachmentCount++] = attachment;
        }
    }
    if (fboInfo.depth) {
        attachments[attachmentCount++] = fboInfo.depth;
    }

    VkFramebufferCreateInfo info {
        .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
        .renderPass = fboInfo.renderPass,
        .attachmentCount = attachmentCount,
        .pAttachments = attachments,
        .width = fboInfo.width,
        .height = fboInfo.height,
        .layers = fboInfo.layers,
    };

    mRenderPassRefCount[info.renderPass]++;
    VkFramebuffer framebuffer;
    VkResult error = vkCreateFramebuffer(mContext.device, &info, VKALLOC, &framebuffer);
    VR_VK_ASSERT(error == VK_SUCCESS, "Unable to create framebuffer.");
    
    mFramebuffers[swapchainIndex] = framebuffer;
    return framebuffer;
}

VkRenderPass VulkanFramebufferCache::getRenderPass(const RenderPassInfo& renderPassInfo, uint32_t swapchainIndex) {
    if (mRenderPasses.size() < VK_MAX_COMMAND_BUFFERS) {
        mRenderPasses.resize(VK_MAX_COMMAND_BUFFERS);
    }
    
    if (mRenderPasses[swapchainIndex] != VK_NULL_HANDLE) {
        return mRenderPasses[swapchainIndex];
    }

    const bool isPresent = renderPassInfo.colorLayout[0] == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    const bool hasSubpasses = renderPassInfo.subpassMask != 0;

    constexpr VkAttachmentLoadOp kClear = VK_ATTACHMENT_LOAD_OP_CLEAR;
    constexpr VkAttachmentLoadOp kDontCare = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    constexpr VkAttachmentLoadOp kKeep = VK_ATTACHMENT_LOAD_OP_LOAD;
    constexpr VkAttachmentStoreOp kDisableStore = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    constexpr VkAttachmentStoreOp kEnableStore = VK_ATTACHMENT_STORE_OP_STORE;

    const bool discard = any(renderPassInfo.discardStart & TargetBufferFlags::COLOR);

    ImageLayoutInfo colorLayouts[MAX_SUPPORTED_RENDER_TARGET_COUNT];

    if (isPresent) {
        colorLayouts[0].subpassLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorLayouts[0].initialLayout = discard ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        colorLayouts[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    } else {
        for (int i = 0; i < MAX_SUPPORTED_RENDER_TARGET_COUNT; i++) {
            colorLayouts[i].subpassLayout = renderPassInfo.colorLayout[i];
            colorLayouts[i].initialLayout = renderPassInfo.colorLayout[i];
            colorLayouts[i].finalLayout = renderPassInfo.colorLayout[i];
        }
    }

    VkAttachmentReference inputAttachmentRef[MAX_SUPPORTED_RENDER_TARGET_COUNT] = {};
    VkAttachmentReference colorAttachmentRefs[2][MAX_SUPPORTED_RENDER_TARGET_COUNT] = {};
    VkAttachmentReference resolveAttachmentRef[MAX_SUPPORTED_RENDER_TARGET_COUNT] = {};
    VkAttachmentReference depthAttachmentRef = {};

    const bool hasDepth = renderPassInfo.depthFormat != VK_FORMAT_UNDEFINED;

    VkSubpassDescription subpasses[2] = {{
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .pInputAttachments = nullptr,
        .pColorAttachments = colorAttachmentRefs[0],
        .pResolveAttachments = resolveAttachmentRef,
        .pDepthStencilAttachment = hasDepth ? &depthAttachmentRef : nullptr
    },
    {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .pInputAttachments = inputAttachmentRef,
        .pColorAttachments = colorAttachmentRefs[1],
        .pResolveAttachments = resolveAttachmentRef,
        .pDepthStencilAttachment = hasDepth ? &depthAttachmentRef : nullptr
    }};

    VkAttachmentDescription attachments[MAX_SUPPORTED_RENDER_TARGET_COUNT + MAX_SUPPORTED_RENDER_TARGET_COUNT + 1] = {};

    // 2 subpasses
    VkSubpassDependency dependencies[1] = {{
        .srcSubpass = 0,
        .dstSubpass = 1,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
        .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
    }};

    VkRenderPassCreateInfo renderPassCreateInfo {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 0u,
        .pAttachments = attachments,
        .subpassCount = hasSubpasses ? 2u : 1u,
        .pSubpasses = subpasses,
        .dependencyCount = hasSubpasses ? 1u : 0u,
        .pDependencies = dependencies
    };

    int attachmentIndex = 0;

    // color attachments
    for (int i = 0; i < MAX_SUPPORTED_RENDER_TARGET_COUNT; i++) {
        if (renderPassInfo.colorFormat[i] == VK_FORMAT_UNDEFINED) {
            continue;
        }
        const VkImageLayout subpassLayout = colorLayouts[i].subpassLayout;
        uint32_t index;

        if (!hasSubpasses) {
            index = subpasses[0].colorAttachmentCount++;
            colorAttachmentRefs[0][index].layout = subpassLayout;
            colorAttachmentRefs[0][index].attachment = attachmentIndex;
        } else {

            VR_ASSERT(renderPassInfo.subpassMask == 1);

            if (renderPassInfo.subpassMask & (1 << i)) {
                index = subpasses[0].colorAttachmentCount++;
                colorAttachmentRefs[0][index].layout = subpassLayout;
                colorAttachmentRefs[0][index].attachment = attachmentIndex;

                index = subpasses[1].inputAttachmentCount++;
                inputAttachmentRef[index].layout = subpassLayout;
                inputAttachmentRef[index].attachment = attachmentIndex;
            }

            index = subpasses[1].colorAttachmentCount++;
            colorAttachmentRefs[1][index].layout = subpassLayout;
            colorAttachmentRefs[1][index].attachment = attachmentIndex;
        }

        const TargetBufferFlags flag = TargetBufferFlags(int(TargetBufferFlags::COLOR0) << i);
        const bool clear = any(renderPassInfo.clear & flag);
        const bool discard = any(renderPassInfo.discardStart & flag);
        // define the behaviors at the begin and end of the renader pass
        attachments[attachmentIndex++] = {
            .format = renderPassInfo.colorFormat[i],
            .samples = (VkSampleCountFlagBits) renderPassInfo.samples,
            .loadOp = clear ? kClear : (discard ? kDontCare : kKeep),
            .storeOp = renderPassInfo.samples == 1 ? kEnableStore : kDisableStore,
            .stencilLoadOp = kDontCare,
            .stencilStoreOp = kDisableStore,
            .initialLayout = colorLayouts[i].initialLayout,
            .finalLayout = colorLayouts[i].finalLayout
        };
    }

    // avoid VK_ERROR_OUT_OF_HOST_MEMORY on Adreno GPU
    if (subpasses[0].colorAttachmentCount == 0) {
        subpasses[0].pColorAttachments = nullptr;
        subpasses[0].pResolveAttachments = nullptr;
        subpasses[1].pColorAttachments = nullptr;
        subpasses[1].pResolveAttachments = nullptr;
    }

    // resolve attachments, for msaa
    VkAttachmentReference* pResolveAttachment = resolveAttachmentRef;
    for (int i = 0; i < MAX_SUPPORTED_RENDER_TARGET_COUNT; i++) {
        if (renderPassInfo.colorFormat[i] == VK_FORMAT_UNDEFINED) {
            continue;
        }

        if (!(renderPassInfo.needsResolveMask & (1 << i))) {
            pResolveAttachment->attachment = VK_ATTACHMENT_UNUSED;
            ++pResolveAttachment;
            continue;
        }

        pResolveAttachment->attachment = attachmentIndex;
        pResolveAttachment->layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        ++pResolveAttachment;

        attachments[attachmentIndex++] = {
            .format = renderPassInfo.colorFormat[i],
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .loadOp = kDontCare,
            .storeOp = kEnableStore,
            .stencilLoadOp = kDontCare,
            .stencilStoreOp = kDisableStore,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .finalLayout = colorLayouts[i].finalLayout
        };
    }

    // depth attachment
    if (hasDepth) {
        bool clear = any(renderPassInfo.clear & TargetBufferFlags::DEPTH);
        bool discard = any(renderPassInfo.discardStart & TargetBufferFlags::DEPTH);
        depthAttachmentRef.layout = renderPassInfo.depthLayout;
        depthAttachmentRef.attachment = attachmentIndex;
        attachments[attachmentIndex++] = {
            .format = renderPassInfo.depthFormat,
            .samples = (VkSampleCountFlagBits) renderPassInfo.samples,
            .loadOp = clear ? kClear : (discard ? kDontCare : kKeep),
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
            .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
            .initialLayout = renderPassInfo.depthLayout,
            .finalLayout = renderPassInfo.depthLayout
        };
    }
    renderPassCreateInfo.attachmentCount = attachmentIndex;

    // create the render pass
    VkRenderPass renderPass;
    VkResult error = vkCreateRenderPass(mContext.device, &renderPassCreateInfo, VKALLOC, &renderPass);
    VR_VK_ASSERT(error == VK_SUCCESS, "Unable to create render pass.");
    
    mRenderPasses[swapchainIndex] = renderPass;
    return renderPass;
}

void VulkanFramebufferCache::reset() {
    for (auto& framebuffer : mFramebuffers) {
        vkDestroyFramebuffer(mContext.device, framebuffer, VKALLOC);
    }
    mFramebuffers.clear();
    
    for (auto& renderpass  : mRenderPasses) {
        vkDestroyRenderPass(mContext.device, renderpass, VKALLOC);
    }
    mRenderPasses.clear();
}

void VulkanFramebufferCache::gc() {
    if (++mCurrentTime <= VK_MAX_COMMAND_BUFFERS) {
        return;
    }
    // TODO
    return;
}

} // namespace backend
} // namespace VR
