 #ifndef VULKAN_PROGRAM_H
 #define VULKAN_PROGRAM_H

#include "Program.h"
#include "VulkanContext.h"

namespace VR {
namespace backend {

class VulkanProgram : public NonCopyable {

public:
    VulkanProgram(VulkanContext& context, const Program& program, std::string programName);
    virtual ~VulkanProgram();
    const std::vector<VkShaderModule>& getShaderModules();
    Program::Sampler& getSamplerBlockInfo(size_t bindingPoint) { return mSamplerBlock[bindingPoint]; }
    std::string& getUniformBlockInfo(size_t bindingPoint) { return mUniformBlock[bindingPoint]; }
    std::string& getProgramName() { return mName; }
    
private:
    VulkanContext& mContext;
    std::vector<VkShaderModule> mShaderModules;
    Program::SamplerBlock mSamplerBlock = {};
    Program::UniformBlock mUniformBlock = {};
    std::string mName;
};

} // namespace backend
} // namespace VR


#endif
