#ifndef VULKAN_PIPELINE_CACHE_H
#define VULKAN_PIPELINE_CACHE_H

#include <map>
#include <vector>
#include "NonCopyable.h"
#include "VulkanMacros.h"
#include "VulkanEnums.h"
#include "VulkanUtils.h"
#include "VulkanCommandPool.h"

namespace VR {
namespace backend {
   
class VulkanPipelineCache : public CommandBufferObserver, NonCopyable {
public:

    struct VertexAttributeArray {
        // position, normal, texcoord ...
        VkVertexInputAttributeDescription attributes[VERTEX_ATTRIBUTE_COUNT] = {};
        // buffer1, buffer2 ...
        VkVertexInputBindingDescription buffers[VERTEX_ATTRIBUTE_COUNT] = {};
    };

    struct RasterState {
        VkPipelineRasterizationStateCreateInfo rasterization;
        VkPipelineColorBlendAttachmentState blending;
        VkPipelineDepthStencilStateCreateInfo depthStencil;
        VkPipelineMultisampleStateCreateInfo multisampling;
        uint32_t colorTargetCount;
    };

    struct PipelineInfo {
        VkShaderModule shaders[SHADER_MODULE_COUNT] = {};
        RasterState rasterState; 
        VkRenderPass renderPass; 
        VkPrimitiveTopology topology;
        uint16_t subpassIndex; 
        VkVertexInputAttributeDescription vertexAttributes[VERTEX_ATTRIBUTE_COUNT] = {};
        VkVertexInputBindingDescription vertexBuffers[VERTEX_ATTRIBUTE_COUNT] = {};
    };

    struct DescriptorInfo {
        VkBuffer uniformBuffers[UBUFFER_BINDING_COUNT] = {}; // 8
        VkDescriptorImageInfo samplers[SAMPLER_BINDING_COUNT] = {}; // 16
        VkDescriptorImageInfo inputAttachments[TARGET_BINDING_COUNT] = {}; // 8
        VkDeviceSize uniformBufferOffsets[UBUFFER_BINDING_COUNT] = {}; // 8
        VkDeviceSize uniformBufferSizes[UBUFFER_BINDING_COUNT] = {}; // 8
    };

    // descriptor sets
    struct DescriptorSetInfo {
        VkDescriptorSet descSets[DESCRIPTOR_TYPE_COUNT] = {};
    };

    struct CmdBufferState {
        VkPipeline currentPipeline = VK_NULL_HANDLE;
        DescriptorSetInfo* currentDescriptorSetInfo = nullptr;
        VkRect2D scissor = {};
    };

    VulkanPipelineCache();
    virtual ~VulkanPipelineCache();

    void setDevice(VkDevice device) { mDevice = device; }
    const RasterState& getDefaultRasterState() const { return mDefaultRasterState; }

    bool bindDescriptorSets(VkCommandBuffer cmdbuffer);
    void bindPipeline(VkCommandBuffer cmdbuffer);
    void bindScissor(VkCommandBuffer cmdbuffer, VkRect2D scissor);
    void bindProgram(const VkShaderModule& vertex, const VkShaderModule& fragment);
    void bindRasterState(const RasterState& rasterState);
    void bindRenderPass(VkRenderPass renderPass, int subpassIndex);
    void bindPrimitiveTopology(VkPrimitiveTopology topology);
    void bindUniformBuffer(uint32_t bindingIndex, VkBuffer uniformBuffer, VkDeviceSize offset = 0, VkDeviceSize size = VK_WHOLE_SIZE);
    void bindSamplers(VkDescriptorImageInfo samplers[SAMPLER_BINDING_COUNT]);
    void bindInputAttachment(uint32_t bindingIndex, VkDescriptorImageInfo imageInfo);
    void bindVertexAttributeArray(const VertexAttributeArray& varray);
    void unbindUniformBuffer(VkBuffer uniformBuffer);
    void unbindImageView(VkImageView imageView);

    void destroyCache();
    void onCommandBuffer(const VulkanCommandBuffer& cmdbuffer) override;

private:

    void createDescriptorSets(VkDescriptorSet descriptors[DESCRIPTOR_TYPE_COUNT], bool* bound);
    bool createPipeline(VkPipeline* pipeline);
    void createLayoutsAndDescriptors();
    void destroyLayoutsAndDescriptors();
    VkDescriptorPool createDescriptorPool(uint32_t size) const;

private:
    VkDevice mDevice = VK_NULL_HANDLE;
    const RasterState mDefaultRasterState = {};

    DescriptorInfo mDescriptorInfo = {};
    uint32_t mDescriptorTypeCount = 0;
    PipelineInfo mPipelineInfo = {};
    
    CmdBufferState mCmdBufferState[VK_MAX_COMMAND_BUFFERS] = {};

    VkDescriptorSetLayout mDescriptorSetLayouts[DESCRIPTOR_TYPE_COUNT] = {};
    std::vector<VkDescriptorSet> mDescriptorSets[DESCRIPTOR_TYPE_COUNT] = {};
    
    VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
    std::vector<VkPipeline> mPipelines = {};
    VkPipelineCache mPipelineCache  = VK_NULL_HANDLE;
    
    uint32_t mCmdBufferIndex = 0;

    VkDescriptorPool mDescriptorPool = {};
    uint32_t mDescriptorPoolSize = 400;
};

} // namespace backend
} // namespace VR

#endif // VULKAN_PIPELINE_CACHE_H
