#include "VulkanPipelineCache.h"
#include "VulkanAlloc.h"
#include "VulkanProgram.h"

namespace VR {
namespace backend {

static VulkanPipelineCache::RasterState createDefaultRasterState();

VulkanPipelineCache::VulkanPipelineCache() : mDefaultRasterState(createDefaultRasterState()) {
    mPipelines.resize(VK_MAX_COMMAND_BUFFERS);
}

VulkanPipelineCache::~VulkanPipelineCache() {
    // Do nothing
}

bool VulkanPipelineCache::bindDescriptorSets(VkCommandBuffer cmdbuffer) {
    VkDescriptorSet descriptors[DESCRIPTOR_TYPE_COUNT];
    bool bound = false;
    createDescriptorSets(descriptors, &bound);
    if (bound && mDescriptorTypeCount != 0) {
        vkCmdBindDescriptorSets(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mPipelineLayout, 0, mDescriptorTypeCount, descriptors, 0, nullptr);
    }
    return true;
}

void VulkanPipelineCache::bindPipeline(VkCommandBuffer cmdbuffer) {
    if (mPipelineCache == VK_NULL_HANDLE) {
        VkPipelineCacheCreateInfo pipelineCacheCreateInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO, };
        VkResult result = vkCreatePipelineCache(mDevice, &pipelineCacheCreateInfo, nullptr, &mPipelineCache);
        VR_VK_ASSERT(result == VK_SUCCESS, "vkCreatePipelineCache error.")
    }
    
    VkPipeline pipeline;
    if (createPipeline(&pipeline)) {
        vkCmdBindPipeline(cmdbuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
    }    
}

void VulkanPipelineCache::bindScissor(VkCommandBuffer cmdbuffer, VkRect2D scissor) {
    VkRect2D& currentScissor = mCmdBufferState[mCmdBufferIndex].scissor;
    if (!equivalent(currentScissor, scissor)) {
        currentScissor = scissor;
        vkCmdSetScissor(cmdbuffer, 0, 1, &scissor);
    }
}

void VulkanPipelineCache::createDescriptorSets(VkDescriptorSet descriptorSets[DESCRIPTOR_TYPE_COUNT], bool* bound) {

    if (!mPipelineLayout) {
        createLayoutsAndDescriptors();
    }

    DescriptorSetInfo*& descriptorSetInfo = mCmdBufferState[mCmdBufferIndex].currentDescriptorSetInfo;
    if (descriptorSetInfo != nullptr) {
        for (uint32_t i = 0; i < mDescriptorTypeCount; ++i) {
            descriptorSets[i] = descriptorSetInfo->descSets[i];
        }
        *bound = true;
        return;
    }
    
    // no cached descriptor sets then create new ones for each type
    if (mDescriptorSets[0].empty()) {
        if (mDescriptorTypeCount == 0) {
            return;
        }
        VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = mDescriptorPool;
        allocInfo.descriptorSetCount = mDescriptorTypeCount;
        allocInfo.pSetLayouts = mDescriptorSetLayouts;
        VkResult result = vkAllocateDescriptorSets(mDevice, &allocInfo, descriptorSets);
        
        VR_ASSERT(result == VK_SUCCESS);
        if (result != VK_SUCCESS) {
            return;
        }
    } else {
        for (uint32_t i = 0; i < mDescriptorTypeCount; ++i) {
            descriptorSets[i] = mDescriptorSets[i].back();
            mDescriptorSets[i].pop_back();
        }
    }
    
    descriptorSetInfo = new DescriptorSetInfo;
    for (uint32_t i = 0; i < mDescriptorTypeCount; ++i) {
        descriptorSetInfo->descSets[i] = descriptorSets[i];
        mDescriptorSets[i].push_back(descriptorSets[i]);
    }
    
    uint32_t uniformDesSize = 0;
    for(auto& uniform : mDescriptorInfo.uniformBuffers) {
        if (uniform != VK_NULL_HANDLE) {
            uniformDesSize++;
        }
    }
    uint32_t samplerDesSize = 0;
    for(auto& sampler : mDescriptorInfo.samplers) {
        if (sampler.sampler != VK_NULL_HANDLE) {
            samplerDesSize++;
        }
    }
    uint32_t inputAttachmentDesSize = 0;
    for(auto& input : mDescriptorInfo.inputAttachments) {
        if (input.sampler != VK_NULL_HANDLE) {
            inputAttachmentDesSize++;
        }
    }
    
    // update descriptorsets
    VkDescriptorBufferInfo descriptorBuffers[uniformDesSize];
    VkDescriptorImageInfo descriptorSamplers[samplerDesSize];
#ifdef VR_VULKAN_SUPPORT_MULTIPASS
    VkDescriptorImageInfo descriptorInputAttachments[inputAttachmentDesSize];
    VkWriteDescriptorSet writeDescriptorSets[uniformDesSize + samplerDesSize + inputAttachmentDesSize];
#else
    VkWriteDescriptorSet writeDescriptorSets[uniformDesSize + samplerDesSize];
#endif
    uint32_t writesCount = 0;
    // Uniform Buffers
    for (uint32_t binding = 0; binding < uniformDesSize; binding++) {
        VkWriteDescriptorSet& writeInfo = writeDescriptorSets[writesCount++];
        if (mDescriptorInfo.uniformBuffers[binding]) {
            VkDescriptorBufferInfo& bufferInfo = descriptorBuffers[binding];
            bufferInfo.buffer = mDescriptorInfo.uniformBuffers[binding];
            bufferInfo.offset = mDescriptorInfo.uniformBufferOffsets[binding];
            bufferInfo.range = mDescriptorInfo.uniformBufferSizes[binding];
            writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeInfo.pNext = nullptr;
            writeInfo.dstArrayElement = 0;
            writeInfo.descriptorCount = 1;
            writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            writeInfo.pImageInfo = nullptr;
            writeInfo.pBufferInfo = &bufferInfo;
            writeInfo.pTexelBufferView = nullptr;
        }
        writeInfo.dstSet = descriptorSetInfo->descSets[0];
        writeInfo.dstBinding = binding;
    }
    // Image Samplers
    for (uint32_t binding = 0; binding < samplerDesSize; binding++) {
        VkWriteDescriptorSet& writeInfo = writeDescriptorSets[writesCount++];
        if (mDescriptorInfo.samplers[binding].sampler) {
            VkDescriptorImageInfo& imageInfo = descriptorSamplers[binding];
            imageInfo = mDescriptorInfo.samplers[binding];
            writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeInfo.pNext = nullptr;
            writeInfo.dstArrayElement = 0;
            writeInfo.descriptorCount = 1;
            writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            writeInfo.pImageInfo = &imageInfo;
            writeInfo.pBufferInfo = nullptr;
            writeInfo.pTexelBufferView = nullptr;
        }
        writeInfo.dstSet = descriptorSetInfo->descSets[1];
        writeInfo.dstBinding = binding;
    }
    
#ifdef VR_VULKAN_SUPPORT_MULTIPASS
    // Input Attachments
    for (uint32_t binding = 0; binding < inputAttachmentDesSize; binding++) {
        VkWriteDescriptorSet& writeInfo = writeDescriptorSets[writesCount++];
        if (mDescriptorInfo.inputAttachments[binding].imageView) {
            VkDescriptorImageInfo& imageInfo = descriptorInputAttachments[binding];
            imageInfo = mDescriptorInfo.inputAttachments[binding];
            writeInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writeInfo.pNext = nullptr;
            writeInfo.dstArrayElement = 0;
            writeInfo.descriptorCount = 1;
            writeInfo.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
            writeInfo.pImageInfo = &imageInfo;
            writeInfo.pBufferInfo = nullptr;
            writeInfo.pTexelBufferView = nullptr;
        }
        writeInfo.dstSet = descriptorSetInfo->descSets[2];
        writeInfo.dstBinding = binding;
    }
#endif
    vkUpdateDescriptorSets(mDevice, writesCount, writeDescriptorSets, 0, nullptr);
    *bound = true;
}

bool VulkanPipelineCache::createPipeline(VkPipeline* pipeline) {

    VR_ASSERT(mPipelineLayout);
    VR_VK_ASSERT(mPipelineInfo.shaders[0] != VK_NULL_HANDLE, "Vertex shader is not bound.");
    VR_VK_ASSERT(mPipelineInfo.shaders[1] != VK_NULL_HANDLE, "Fragment shader is not bound.");
    
    VkPipeline& currentPipeline = mCmdBufferState[mCmdBufferIndex].currentPipeline;

    if (currentPipeline != VK_NULL_HANDLE) {
        *pipeline = currentPipeline;
        return true;
    }

    VkPipelineShaderStageCreateInfo shaderStages[SHADER_MODULE_COUNT];
    shaderStages[0] = VkPipelineShaderStageCreateInfo{};
    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].pName = "main";

    shaderStages[1] = VkPipelineShaderStageCreateInfo{};
    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].pName = "main";

    VkPipelineColorBlendAttachmentState colorBlendAttachments[MAX_SUPPORTED_RENDER_TARGET_COUNT];
    VkPipelineColorBlendStateCreateInfo colorBlendState;
    colorBlendState = VkPipelineColorBlendStateCreateInfo{};
    colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlendState.attachmentCount = 1;
    colorBlendState.pAttachments = colorBlendAttachments;

    shaderStages[0].module = mPipelineInfo.shaders[0];
    shaderStages[1].module = mPipelineInfo.shaders[1];

    uint32_t numVertexAttribs = 0;
    uint32_t numVertexBuffers = 0;
    // use available atrributes
    for (uint32_t i = 0; i < VERTEX_ATTRIBUTE_COUNT; i++) {
        if (mPipelineInfo.vertexAttributes[i].format > 0) {
            numVertexAttribs++;
        }
        if (mPipelineInfo.vertexBuffers[i].stride > 0) {
            numVertexBuffers++;
        }
    }

    VkPipelineVertexInputStateCreateInfo vertexInputState = {};
    vertexInputState.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputState.vertexBindingDescriptionCount = numVertexBuffers;
    vertexInputState.pVertexBindingDescriptions = mPipelineInfo.vertexBuffers;
    vertexInputState.vertexAttributeDescriptionCount = numVertexAttribs;
    vertexInputState.pVertexAttributeDescriptions = mPipelineInfo.vertexAttributes;

    VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
    inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssemblyState.topology = mPipelineInfo.topology;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkDynamicState dynamicStateEnables[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.pDynamicStates = dynamicStateEnables;
    dynamicState.dynamicStateCount = 2;

    VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.layout = mPipelineLayout;
    pipelineCreateInfo.renderPass = mPipelineInfo.renderPass;
    pipelineCreateInfo.subpass = mPipelineInfo.subpassIndex;
    pipelineCreateInfo.stageCount = SHADER_MODULE_COUNT;
    pipelineCreateInfo.pStages = shaderStages;
    pipelineCreateInfo.pVertexInputState = &vertexInputState;
    pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
    pipelineCreateInfo.pRasterizationState = &mPipelineInfo.rasterState.rasterization;
    pipelineCreateInfo.pColorBlendState = &colorBlendState;
    pipelineCreateInfo.pMultisampleState = &mPipelineInfo.rasterState.multisampling;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pDepthStencilState = &mPipelineInfo.rasterState.depthStencil;
    pipelineCreateInfo.pDynamicState = &dynamicState;

    colorBlendState.attachmentCount = mPipelineInfo.rasterState.colorTargetCount;
    for (auto& target : colorBlendAttachments) {
        target = mPipelineInfo.rasterState.blending;
    }

    VkResult result = vkCreateGraphicsPipelines(mDevice, mPipelineCache, 1, &pipelineCreateInfo, VKALLOC, pipeline);
    VR_VK_ASSERT(result == VK_SUCCESS, "vkCreateGraphicsPipelines error.");
    mPipelines[mCmdBufferIndex] = *pipeline;
    currentPipeline = *pipeline;
    
    return true;
}

void VulkanPipelineCache::bindProgram(const VkShaderModule& vertex, const VkShaderModule& fragment) {
    const VkShaderModule shaders[2] = { vertex, fragment };
    for (uint32_t i = 0; i < SHADER_MODULE_COUNT; i++) {
        if (mPipelineInfo.shaders[i] != shaders[i]) {
            mPipelineInfo.shaders[i] = shaders[i];
        }
    }
}

void VulkanPipelineCache::bindRasterState(const RasterState& rasterState) {

    VkPipelineRasterizationStateCreateInfo& raster0 = mPipelineInfo.rasterState.rasterization;
    const VkPipelineRasterizationStateCreateInfo& raster1 = rasterState.rasterization;
    VkPipelineColorBlendAttachmentState& blend0 = mPipelineInfo.rasterState.blending;
    const VkPipelineColorBlendAttachmentState& blend1 = rasterState.blending;
    VkPipelineDepthStencilStateCreateInfo& ds0 = mPipelineInfo.rasterState.depthStencil;
    const VkPipelineDepthStencilStateCreateInfo& ds1 = rasterState.depthStencil;
    VkPipelineMultisampleStateCreateInfo& ms0 = mPipelineInfo.rasterState.multisampling;
    const VkPipelineMultisampleStateCreateInfo& ms1 = rasterState.multisampling;
    if (
            mPipelineInfo.rasterState.colorTargetCount != rasterState.colorTargetCount ||
            raster0.polygonMode != raster1.polygonMode ||
            raster0.cullMode != raster1.cullMode ||
            raster0.frontFace != raster1.frontFace ||
            raster0.rasterizerDiscardEnable != raster1.rasterizerDiscardEnable ||
            raster0.depthBiasEnable != raster1.depthBiasEnable ||
            raster0.depthBiasConstantFactor != raster1.depthBiasConstantFactor ||
            raster0.depthBiasSlopeFactor != raster1.depthBiasSlopeFactor ||
            blend0.colorWriteMask != blend1.colorWriteMask ||
            blend0.blendEnable != blend1.blendEnable ||
            ds0.depthTestEnable != ds1.depthTestEnable ||
            ds0.depthWriteEnable != ds1.depthWriteEnable ||
            ds0.depthCompareOp != ds1.depthCompareOp ||
            ds0.stencilTestEnable != ds1.stencilTestEnable ||
            ms0.rasterizationSamples != ms1.rasterizationSamples ||
            ms0.alphaToCoverageEnable != ms1.alphaToCoverageEnable
    ) {
        mPipelineInfo.rasterState = rasterState;
    }
}

void VulkanPipelineCache::bindRenderPass(VkRenderPass renderPass, int subpassIndex) {
    if (mPipelineInfo.renderPass != renderPass || mPipelineInfo.subpassIndex != subpassIndex) {
        mPipelineInfo.renderPass = renderPass;
        mPipelineInfo.subpassIndex = subpassIndex;
    }
}

void VulkanPipelineCache::bindPrimitiveTopology(VkPrimitiveTopology topology) {
    if (mPipelineInfo.topology != topology) {
        mPipelineInfo.topology = topology;
    }
}

void VulkanPipelineCache::bindVertexAttributeArray(const VertexAttributeArray& varray) {
    for (size_t i = 0; i < VERTEX_ATTRIBUTE_COUNT; i++) {
        VkVertexInputAttributeDescription& attribDst = mPipelineInfo.vertexAttributes[i];
        const VkVertexInputAttributeDescription& attribSrc = varray.attributes[i];
        if (attribSrc.location != attribDst.location || attribSrc.binding != attribDst.binding ||
                attribSrc.format != attribDst.format || attribSrc.offset != attribDst.offset) {
            attribDst.format = attribSrc.format;
            attribDst.binding = attribSrc.binding;
            attribDst.location = attribSrc.location;
            attribDst.offset = attribSrc.offset;
        }
        VkVertexInputBindingDescription& bufferDst = mPipelineInfo.vertexBuffers[i];
        const VkVertexInputBindingDescription& bufferSrc = varray.buffers[i];
        if (bufferDst.binding != bufferSrc.binding || bufferDst.stride != bufferSrc.stride) {
            bufferDst.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
            bufferDst.binding = bufferSrc.binding;
            bufferDst.stride = bufferSrc.stride;
        }
    }
}

void VulkanPipelineCache::unbindUniformBuffer(VkBuffer uniformBuffer) {
    auto& dpInfo = mDescriptorInfo;
    for (uint32_t bindingIndex = 0u; bindingIndex < UBUFFER_BINDING_COUNT; ++bindingIndex) {
        if (dpInfo.uniformBuffers[bindingIndex] == uniformBuffer) {
            dpInfo.uniformBuffers[bindingIndex] = {};
            dpInfo.uniformBufferSizes[bindingIndex] = {};
            dpInfo.uniformBufferOffsets[bindingIndex] = {};
        }
    }
}

void VulkanPipelineCache::unbindImageView(VkImageView imageView) {
    for (auto& sampler : mDescriptorInfo.samplers) {
        if (sampler.imageView == imageView) {
            sampler = {};
        }
    }
#ifdef VR_VULKAN_SUPPORT_MULTIPASS
    for (auto& target : mDescriptorInfo.inputAttachments) {
        if (target.imageView == imageView) {
            target = {};
        }
    }
#endif
}

void VulkanPipelineCache::bindUniformBuffer(uint32_t bindingIndex, VkBuffer uniformBuffer, VkDeviceSize offset, VkDeviceSize size) {
   
    VR_VK_ASSERT(bindingIndex < UBUFFER_BINDING_COUNT, "Uniform bindings out of range.");
    
    auto& dpInfo = mDescriptorInfo;

    if (dpInfo.uniformBuffers[bindingIndex] != uniformBuffer ||
        dpInfo.uniformBufferOffsets[bindingIndex] != offset ||
        dpInfo.uniformBufferSizes[bindingIndex] != size) 
    {
        dpInfo.uniformBuffers[bindingIndex] = uniformBuffer;
        dpInfo.uniformBufferOffsets[bindingIndex] = offset;
        dpInfo.uniformBufferSizes[bindingIndex] = size;
    }
}

void VulkanPipelineCache::bindSamplers(VkDescriptorImageInfo samplers[SAMPLER_BINDING_COUNT]) {
    
    for (uint32_t bindingIndex = 0; bindingIndex < SAMPLER_BINDING_COUNT; bindingIndex++) {
        const VkDescriptorImageInfo& requested = samplers[bindingIndex];
        VkDescriptorImageInfo& existing = mDescriptorInfo.samplers[bindingIndex];
        if (existing.sampler != requested.sampler ||
            existing.imageView != requested.imageView ||
            existing.imageLayout != requested.imageLayout) 
        {
            existing = requested;
        }
    }
}

void VulkanPipelineCache::bindInputAttachment(uint32_t bindingIndex, VkDescriptorImageInfo targetInfo) {
    
    VR_VK_ASSERT(bindingIndex < TARGET_BINDING_COUNT, "Input attachment bindings out of range.");
#ifdef VR_VULKAN_SUPPORT_MULTIPASS
    VkDescriptorImageInfo& imageInfo = mDescriptorInfo.inputAttachments[bindingIndex];
    if (imageInfo.imageView != targetInfo.imageView || imageInfo.imageLayout != targetInfo.imageLayout) {
        imageInfo = targetInfo;
    }
#endif
}

void VulkanPipelineCache::destroyCache() {
    
    for (auto& shaderModule : mPipelineInfo.shaders) {
        if (shaderModule != VK_NULL_HANDLE) {
            vkDestroyShaderModule(mDevice, shaderModule, VKALLOC);
            shaderModule = VK_NULL_HANDLE;
        }
        // DELETE_SHADER_MODULE(mDevice, shaderModule, VKALLOC);
    }

    for (int i = 0; i < VK_MAX_COMMAND_BUFFERS; i++) {
        if (mCmdBufferState[i].currentPipeline != nullptr) {
             vkDestroyPipeline(mDevice, mCmdBufferState[i].currentPipeline, VKALLOC);
             mCmdBufferState[i].currentPipeline = nullptr;
        }
    }

    mPipelines.clear();
    vkDestroyPipelineCache(mDevice, mPipelineCache, VKALLOC);
    
    destroyLayoutsAndDescriptors();
}

void VulkanPipelineCache::onCommandBuffer(const VulkanCommandBuffer& cmdbuffer) {

    mCmdBufferIndex = cmdbuffer.cmdBufferIndex;
    mCmdBufferState[mCmdBufferIndex].scissor = {};
    // TODO: gc
}

void VulkanPipelineCache::createLayoutsAndDescriptors() {
    
    uint32_t setLayoutCount = 0;
    uint32_t uniformDesSize = 0;
    for(auto& uniform : mDescriptorInfo.uniformBuffers) {
        if (uniform != VK_NULL_HANDLE) {
            uniformDesSize++;
        }
    }
    if (uniformDesSize != 0) {
        setLayoutCount++;
    }
    uint32_t samplerDesSize = 0;
    for(auto& sampler : mDescriptorInfo.samplers) {
        if (sampler.sampler != VK_NULL_HANDLE) {
            samplerDesSize++;
        }
    }
    if (samplerDesSize != 0) {
        setLayoutCount++;
    }
    uint32_t inputAttachmentDesSize = 0;
#ifdef VR_VULKAN_SUPPORT_MULTIPASS
    for(auto& input : mDescriptorInfo.inputAttachments) {
        if (input.sampler != VK_NULL_HANDLE) {
         inputAttachmentDesSize++;
        }
    }
#endif
    if (inputAttachmentDesSize != 0) {
        setLayoutCount++;
    }
    mDescriptorTypeCount = setLayoutCount;
    
    VkDescriptorSetLayoutBinding binding = {};
    binding.descriptorCount = 1; 
    binding.stageFlags = VK_SHADER_STAGE_ALL_GRAPHICS; 

    // Uniform Buffers
    VkDescriptorSetLayoutBinding uniformBindings[uniformDesSize];
    binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    for (uint32_t i = 0; i < uniformDesSize; i++) {
        binding.binding = i;
        uniformBindings[i] = binding;
    }
    VkDescriptorSetLayoutCreateInfo dlinfo = {};
    dlinfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dlinfo.bindingCount = uniformDesSize;
    dlinfo.pBindings = uniformBindings;
    if (uniformDesSize != 0) {
        vkCreateDescriptorSetLayout(mDevice, &dlinfo, VKALLOC, &mDescriptorSetLayouts[0]);
    }

    // Image Samplers
    VkDescriptorSetLayoutBinding samplerBindings[samplerDesSize];
    binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    for (uint32_t i = 0; i < samplerDesSize; i++) {
        binding.binding = i;
        samplerBindings[i] = binding;
    }
    dlinfo.bindingCount = samplerDesSize;
    dlinfo.pBindings = samplerBindings;
    if (samplerDesSize != 0) {
        vkCreateDescriptorSetLayout(mDevice, &dlinfo, VKALLOC, &mDescriptorSetLayouts[1]);
    }
    
#ifdef VR_VULKAN_SUPPORT_MULTIPASS
    // Input Attachments
    VkDescriptorSetLayoutBinding tbindings[inputAttachmentDesSize];
    binding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    for (uint32_t i = 0; i < inputAttachmentDesSize; i++) {
        binding.binding = i;
        tbindings[i] = binding;
    }
    dlinfo.bindingCount = inputAttachmentDesSize;
    dlinfo.pBindings = tbindings;
    vkCreateDescriptorSetLayout(mDevice, &dlinfo, VKALLOC, &mDescriptorSetLayouts[2]);
#endif
    // create pipeline
    VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
    pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pPipelineLayoutCreateInfo.setLayoutCount = setLayoutCount;
    pPipelineLayoutCreateInfo.pSetLayouts = mDescriptorSetLayouts;
    VkResult err = vkCreatePipelineLayout(mDevice, &pPipelineLayoutCreateInfo, VKALLOC, &mPipelineLayout);
    VR_VK_CHECK(err == VK_SUCCESS, "Unable to create pipeline layout.");
    // create descriptor pool
    mDescriptorPool = createDescriptorPool(mDescriptorPoolSize);
}

VkDescriptorPool VulkanPipelineCache::createDescriptorPool(uint32_t size) const {

    VkDescriptorPoolSize poolSizes[DESCRIPTOR_TYPE_COUNT] = {};
    VkDescriptorPoolCreateInfo poolInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = size * DESCRIPTOR_TYPE_COUNT,
        .poolSizeCount = DESCRIPTOR_TYPE_COUNT,
        .pPoolSizes = poolSizes
    };
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = poolInfo.maxSets * UBUFFER_BINDING_COUNT;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[1].descriptorCount = poolInfo.maxSets * SAMPLER_BINDING_COUNT;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    poolSizes[2].descriptorCount = poolInfo.maxSets * TARGET_BINDING_COUNT;

    VkDescriptorPool pool;
    VkResult result = vkCreateDescriptorPool(mDevice, &poolInfo, VKALLOC, &pool);
    VR_ASSERT(result == VK_SUCCESS);
    return pool;
}

void VulkanPipelineCache::destroyLayoutsAndDescriptors() {
    if (mPipelineLayout == VK_NULL_HANDLE) {
        return;
    }

    for (auto& descriptorSet : mDescriptorSets) {
        descriptorSet.clear();
    }

    vkDestroyPipelineLayout(mDevice, mPipelineLayout, VKALLOC);
    mPipelineLayout = VK_NULL_HANDLE;

    for (int i = 0; i < DESCRIPTOR_TYPE_COUNT; i++) {
        vkDestroyDescriptorSetLayout(mDevice, mDescriptorSetLayouts[i], VKALLOC);
        mDescriptorSetLayouts[i] = {};
    }

    vkDestroyDescriptorPool(mDevice, mDescriptorPool, VKALLOC);
    mDescriptorPool = VK_NULL_HANDLE;
    for (int i = 0; i < VK_MAX_COMMAND_BUFFERS; i++) {
        delete mCmdBufferState[i].currentDescriptorSetInfo;
        mCmdBufferState[i].currentDescriptorSetInfo = nullptr;
    }
}

static VulkanPipelineCache::RasterState createDefaultRasterState() {

    VkPipelineRasterizationStateCreateInfo rasterization = {};
    rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization.cullMode = VK_CULL_MODE_NONE;
    rasterization.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    rasterization.depthClampEnable = VK_FALSE;
    rasterization.rasterizerDiscardEnable = VK_FALSE;
    rasterization.depthBiasEnable = VK_FALSE;
    rasterization.depthBiasConstantFactor = 0.0f;
    rasterization.depthBiasClamp = 0.0f; 
    rasterization.depthBiasSlopeFactor = 0.0f;
    rasterization.lineWidth = 1.0f;

    VkPipelineColorBlendAttachmentState blending = {};
    blending.colorWriteMask = 0xf;
    blending.blendEnable = VK_FALSE;

    VkPipelineDepthStencilStateCreateInfo depthStencil = {};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = VK_TRUE;
    depthStencil.depthWriteEnable = VK_TRUE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.back.failOp = VK_STENCIL_OP_KEEP;
    depthStencil.back.passOp = VK_STENCIL_OP_KEEP;
    depthStencil.back.compareOp = VK_COMPARE_OP_ALWAYS;
    depthStencil.stencilTestEnable = VK_FALSE;
    depthStencil.front = depthStencil.back;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = true;

    return VulkanPipelineCache::RasterState {
        rasterization,
        blending,
        depthStencil,
        multisampling,
        1,
    };
}

} // namespace backend
} // namespace VR
