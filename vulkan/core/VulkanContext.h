#ifndef VULKAN_CONTEXT_H
#define VULKAN_CONTEXT_H

#include <memory>
#include <vector>
#include <algorithm>

#include "NonCopyable.h"

#include "VulkanMacros.h"
#include "VulkanEnums.h"
#include "VulkanUtils.h"

#include "VulkanCommandPool.h"
#include "VulkanPipelineCache.h"

#include "VulkanWrapper.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wall"
#pragma clang diagnostic ignored "-Wextra"
#pragma clang diagnostic ignored "-Wdeprecated"
#pragma clang diagnostic ignored "-Wundef"
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wmissing-field-initializers"
#pragma clang diagnostic ignored "-Wtautological-compare"
#pragma clang diagnostic ignored "-Wnullability-completeness"
#pragma clang diagnostic ignored "-Wweak-vtables"
#pragma clang diagnostic ignored "-Wimplicit-fallthrough"
#include "vk_mem_alloc.h"
#pragma clang diagnostic pop

#include "VulkanAlloc.h"

namespace VR {
namespace backend {

struct VulkanRenderTarget;
struct VulkanSwapChain;
struct VulkanTexture;
class VulkanMemoryPool;

struct VulkanAttachment {
    VkFormat format;
    VkImage image;
    VkImageView view;
    VkDeviceMemory memory;
    VulkanTexture* texture = nullptr;
    VkImageLayout layout;
    uint8_t baseMipLevel = 0u;
    uint16_t baseLayer = 0u;
};

struct VulkanRenderPass {
    VkRenderPass renderPass;
    uint32_t subpassMask;
    int currentSubpass;
    VulkanTexture* depthFeedback;
};

struct VulkanContext {
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkPhysicalDeviceProperties physicalDeviceProperties;
    VkPhysicalDeviceFeatures physicalDeviceFeatures;
    VkPhysicalDeviceMemoryProperties memoryProperties;
    VkDevice device;
    VkCommandPool commandPool;
    uint32_t graphicsQueueFamilyIndex;
    VkQueue graphicsQueue;
    bool debugMarkersSupported;
    bool debugUtilsSupported;
    bool portabilitySubsetSupported;
    bool maintenanceSupported[3];
    VulkanPipelineCache::RasterState rasterState;
    VulkanSwapChain* currentSwapChain;
    VulkanRenderPass currentRenderPass;
    VkViewport viewport;
    VkFormat depthFormat;
    VmaAllocator allocator;
    VulkanTexture* emptyTexture = nullptr;
    VulkanCommandPool* commandpool = nullptr;
};

void selectPhysicalDevice(VulkanContext& context);
void createLogicalDevice(VulkanContext& context);

uint32_t selectMemoryType(VulkanContext& context, uint32_t flags, VkFlags reqs);
VulkanAttachment& getSwapChainColorAttachment(VulkanContext& context);
VulkanAttachment& getSwapChainDepthAttachment(VulkanContext& context);
uint32_t getSwapChainIndex(VulkanContext& context);
void waitForIdle(VulkanContext& context);
VkFormat findSupportedFormat(VulkanContext& context, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features);
VkImageLayout getTextureLayout(TextureUsage usage);

} // namespace backend
} // namespace VR

#endif // VULKAN_CONTEXT_H
