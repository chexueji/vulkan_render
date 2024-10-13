#include "VulkanProgram.h"

namespace VR {
namespace backend {

VulkanProgram::VulkanProgram(VulkanContext& context, const Program& program, std::string programName): mContext(context), mName(programName) {
    
    auto const& shaderBins = program.getShadersSource();  
    VR_ASSERT(SHADER_TYPE_COUNT == 2);

    mShaderModules.resize(SHADER_TYPE_COUNT);
    // shader bin: should already be compiled in spirv
    for (size_t i = 0; i < SHADER_TYPE_COUNT; i++) {
        const auto& shaderBin = shaderBins[i];
        VkShaderModule &shaderModule = mShaderModules[i];
        VR_ASSERT(!shaderBin.empty());
       
        VkShaderModuleCreateInfo shaderModuleInfo = {};
        shaderModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        shaderModuleInfo.codeSize = shaderBin.size();
        shaderModuleInfo.pCode = (uint32_t*) shaderBin.data();
        VkResult err = vkCreateShaderModule(mContext.device, &shaderModuleInfo, VKALLOC, &shaderModule);
        VR_VK_CHECK(err == VK_SUCCESS, "Create shader module failed.");       
    }
    
    if (program.hasSamplers()) {
        mSamplerBlock = program.getSamplerBlocks();
    } else {
        mSamplerBlock.resize(SAMPLER_BINDING_COUNT);
    }
    
    mUniformBlock = program.getUniformBlocks();
}

VulkanProgram::~VulkanProgram() {
    DELETE_SHADER_MODULE(mContext.device, mShaderModules[0], VKALLOC);
    DELETE_SHADER_MODULE(mContext.device, mShaderModules[1], VKALLOC);
}

const std::vector<VkShaderModule>& VulkanProgram::getShaderModules()
{
    return mShaderModules;
}

} // namespace backend
} // namespace VR
