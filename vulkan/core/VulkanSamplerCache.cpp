#include "VulkanSamplerCache.h"

namespace VR {
namespace backend {

// helper functions
constexpr inline VkSamplerAddressMode getWrapMode(SamplerWrapMode mode) {
    switch (mode) {
        case SamplerWrapMode::REPEAT:
            return VK_SAMPLER_ADDRESS_MODE_REPEAT;
        case SamplerWrapMode::CLAMP_TO_EDGE:
            return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        case SamplerWrapMode::MIRRORED_REPEAT:
            return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    }
}

constexpr inline VkFilter getFilter(SamplerMinFilter filter) {
    switch (filter) {
        case SamplerMinFilter::NEAREST:
            return VK_FILTER_NEAREST;
        case SamplerMinFilter::LINEAR:
            return VK_FILTER_LINEAR;
        case SamplerMinFilter::NEAREST_MIPMAP_NEAREST:
            return VK_FILTER_NEAREST;
        case SamplerMinFilter::LINEAR_MIPMAP_NEAREST:
            return VK_FILTER_LINEAR;
        case SamplerMinFilter::NEAREST_MIPMAP_LINEAR:
            return VK_FILTER_NEAREST;
        case SamplerMinFilter::LINEAR_MIPMAP_LINEAR:
            return VK_FILTER_LINEAR;
    }
}

constexpr inline VkFilter getFilter(SamplerMagFilter filter) {
    switch (filter) {
        case SamplerMagFilter::NEAREST:
            return VK_FILTER_NEAREST;
        case SamplerMagFilter::LINEAR:
            return VK_FILTER_LINEAR;
    }
}

constexpr inline VkSamplerMipmapMode getMipmapMode(SamplerMinFilter filter) {
    switch (filter) {
        case SamplerMinFilter::NEAREST:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case SamplerMinFilter::LINEAR:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case SamplerMinFilter::NEAREST_MIPMAP_NEAREST:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case SamplerMinFilter::LINEAR_MIPMAP_NEAREST:
            return VK_SAMPLER_MIPMAP_MODE_NEAREST;
        case SamplerMinFilter::NEAREST_MIPMAP_LINEAR:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
        case SamplerMinFilter::LINEAR_MIPMAP_LINEAR:
            return VK_SAMPLER_MIPMAP_MODE_LINEAR;
    }
}

constexpr inline float getMaxLod(SamplerMinFilter filter) {
    switch (filter) {
        case SamplerMinFilter::NEAREST:
        case SamplerMinFilter::LINEAR:
            // use a max LOD of 0.25 to "disable" mipmapping.
            return 0.25f;
        case SamplerMinFilter::NEAREST_MIPMAP_NEAREST:
        case SamplerMinFilter::LINEAR_MIPMAP_NEAREST:
        case SamplerMinFilter::NEAREST_MIPMAP_LINEAR:
        case SamplerMinFilter::LINEAR_MIPMAP_LINEAR:
            // 12 miplevels
            return 12.0f;
    }
}

constexpr inline VkBool32 getCompareEnable(SamplerCompareMode mode) {
    return mode == SamplerCompareMode::NONE ? VK_FALSE : VK_TRUE;
}

VulkanSamplerCache::VulkanSamplerCache(VulkanContext& context) : mContext(context) {}

VkSampler VulkanSamplerCache::getSampler(backend::SamplerParams params) {
    auto iter = mSamplerCache.find(params.u);
    if (iter != mSamplerCache.end()) {
        return iter->second;
    }
    
    VkSamplerCreateInfo samplerInfo {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = getFilter(params.filterMag),
        .minFilter = getFilter(params.filterMin),
        .mipmapMode = getMipmapMode(params.filterMin),
        .addressModeU = getWrapMode(params.wrapS),
        .addressModeV = getWrapMode(params.wrapT),
        .addressModeW = getWrapMode(params.wrapR),
        .anisotropyEnable = params.anisotropyLog2 == 0 ? 0u : 1u,
        .maxAnisotropy = (float)(1u << params.anisotropyLog2),
        .compareEnable = getCompareEnable(params.compareMode),
        .compareOp = getCompareOp(params.compareFunc),
        .minLod = 0.0f,
        .maxLod = getMaxLod(params.filterMin),
        .borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE
    };
    VkSampler sampler;
    VkResult error = vkCreateSampler(mContext.device, &samplerInfo, VKALLOC, &sampler);
    VR_VK_ASSERT(error == VK_SUCCESS, "Create sampler failed.");
    mSamplerCache.insert({params.u, sampler});
    return sampler;
}

void VulkanSamplerCache::reset() {
    for (auto pair : mSamplerCache) {
        vkDestroySampler(mContext.device, pair.second, VKALLOC);
    }
    mSamplerCache.clear();
}

} // namespace backend
} // namespace VR
