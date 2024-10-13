#ifndef VULKAN_SAMPLER_CACHE_H
#define VULKAN_SAMPLER_CACHE_H

#include <map>
#include "NonCopyable.h"
#include "VulkanContext.h"
#include "VulkanTexture.h"

namespace VR {
namespace backend {

struct VulkanSampler {
    std::shared_ptr<VulkanTexture> texture = nullptr;
    SamplerParams samplerParam;
    VulkanSampler() = default;
    VulkanSampler(std::shared_ptr<VulkanTexture> tex, SamplerParams& sp): texture(tex), samplerParam(std::move(sp)) {};
};

class VulkanSamplerCache : public NonCopyable {
public:
    VulkanSamplerCache(VulkanContext& context);
    VkSampler getSampler(SamplerParams params);
    void reset();
private:
    VulkanContext& mContext;
    std::map<uint32_t, VkSampler> mSamplerCache;
};

} // namespace backend
} // namespace VR

#endif // VULKAN_SAMPLER_CACHE_H
