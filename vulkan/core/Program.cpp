#include "Program.h"

namespace VR {
namespace backend {

Program& Program::shader(Program::Shader shader, void const* data, size_t size) {
    std::vector<uint8_t> blob(size);
    std::copy_n((const uint8_t *)data, size, blob.data());
    mShadersSource[size_t(shader)] = std::move(blob);
    return *this;
}

Program& Program::setUniforms(size_t bindingPoint, std::string uniformName) {
    mUniformBlock[bindingPoint] = std::move(uniformName);
    return *this;
}

Program& Program::setSamplers(size_t bindingPoint, Program::Sampler sampler) {
    auto& samplerBlock = mSamplerBlock[bindingPoint];
    mSamplerBlock[bindingPoint] = sampler;
    mHasSamplers = true;
    return *this;
}

} // namespace backend
} // namespace VR
