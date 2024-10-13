//
//  Created by xueji.che.
//
#ifndef PROGRAM_H
#define PROGRAM_H

#include <array>
#include <vector>
#include <string>
#include "NonCopyable.h"
#include "VulkanMacros.h"

namespace VR {
namespace backend {

class Program : public NonCopyable {
public:

    enum class Shader : uint8_t {
        VERTEX = 0,
        FRAGMENT = 1
    };

    struct Sampler {
        std::string name = "";
        uint16_t binding = 0;
        Sampler() = default;
        Sampler(std::string _name, uint16_t _binding): name(std::move(_name)), binding(_binding) {}
    };

    using SamplerBlock = std::vector<Sampler>;
    using UniformBlock = std::vector<std::string>;

    Program() {mUniformBlock.resize(BINDING_COUNT); mSamplerBlock.resize(SAMPLER_BINDING_COUNT);};
    virtual ~Program() = default;

    Program& shader(Shader shader, void const* data, size_t size);
    Program& setUniforms(size_t bindingPoint, std::string uniformName);
    Program& setSamplers(size_t bindingPoint, Sampler sampler);
    Program& withVertexShader(void const* data, size_t size) {
        return shader(Shader::VERTEX, data, size);
    }
    Program& withFragmentShader(void const* data, size_t size) {
        return shader(Shader::FRAGMENT, data, size);
    }
    std::array<std::vector<uint8_t>, SHADER_TYPE_COUNT> const& getShadersSource() const {
        return mShadersSource;
    }
    UniformBlock const& getUniformBlocks() const { return mUniformBlock; }
    SamplerBlock const& getSamplerBlocks() const { return mSamplerBlock; }
    const std::string& getName() const { return mName; }
    bool hasSamplers() const { return mHasSamplers; }

private:

    UniformBlock mUniformBlock;
    SamplerBlock mSamplerBlock;
    std::array<std::vector<uint8_t>, SHADER_TYPE_COUNT> mShadersSource;
    std::string mName;
    bool mHasSamplers = false;
};

} // namespace backend;
} // namespace VR;

#endif // PROGRAM_H
