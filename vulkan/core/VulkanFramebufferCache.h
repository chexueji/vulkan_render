#ifndef VULKAN_FRAMEBUFFER_CACHE_H
#define VULKAN_FRAMEBUFFER_CACHE_H

#include <map>
#include <string>
#include "VulkanContext.h"

namespace VR {
namespace backend {

class VulkanFramebufferCache : public NonCopyable {
public:

    struct ImageLayoutInfo { 
        VkImageLayout subpassLayout;
        VkImageLayout initialLayout;
        VkImageLayout finalLayout; 
    }; 

    struct RenderPassInfo {
        VkImageLayout colorLayout[MAX_SUPPORTED_RENDER_TARGET_COUNT];
        VkFormat colorFormat[MAX_SUPPORTED_RENDER_TARGET_COUNT];
        VkImageLayout depthLayout;
        VkFormat depthFormat;
        TargetBufferFlags clear;
        TargetBufferFlags discardStart;
        TargetBufferFlags discardEnd;
        uint8_t samples;
        uint8_t needsResolveMask;
        uint8_t subpassMask;
    };
    
    struct FrameBufferInfo {
        VkRenderPass renderPass;
        uint16_t width; 
        uint16_t height; 
        uint16_t layers; 
        uint16_t samples;
        VkImageView color[MAX_SUPPORTED_RENDER_TARGET_COUNT]; 
        VkImageView resolve[MAX_SUPPORTED_RENDER_TARGET_COUNT]; // for msaa with color which is the src image of msaa
        VkImageView depth;
    };

    VulkanFramebufferCache(VulkanContext& context);
    virtual ~VulkanFramebufferCache();

    VkFramebuffer getFramebuffer(const FrameBufferInfo& fboInfo, uint32_t swapchainIndex);
    VkRenderPass getRenderPass(const RenderPassInfo& fboInfo, uint32_t swapchainIndex);
    void gc();
    void reset();

private:
    VulkanContext& mContext;
    std::map<VkRenderPass, uint32_t> mRenderPassRefCount;
    uint32_t mCurrentTime = 0;
    
    std::vector<VkFramebuffer> mFramebuffers;
    std::vector<VkRenderPass> mRenderPasses;
    
};

} // namespace backend
} // namespace VR

#endif // VULKAN_FRAMEBUFFER_CACHE_H
