#ifndef VULKAN_UTILITY_H
#define VULKAN_UTILITY_H

#include <type_traits>
#include <stddef.h>
#include <stdint.h>
#include "VulkanEnums.h"
#include "VulkanMacros.h"
#include "VulkanWrapper.h"

namespace VR {
namespace backend {

#define DELETE_PTR(p) if (p != nullptr) {delete p; p = nullptr;}

#define DELETE_SHADER_MODULE(device, module, allocator) if (device != VK_NULL_HANDLE && module != VK_NULL_HANDLE) \
                                    { vkDestroyShaderModule(device, module, allocator); module = VK_NULL_HANDLE; }

struct VulkanLayoutTransition {
    VkImage image;
    VkImageLayout oldLayout;
    VkImageLayout newLayout;
    VkImageSubresourceRange subresources;
    VkPipelineStageFlags srcStage;
    VkAccessFlags srcAccessMask;
    VkPipelineStageFlags dstStage;
    VkAccessFlags dstAccessMask;
};

void createSemaphore(VkDevice device, VkSemaphore* semaphore);
VkFormat getVkFormat(ElementType type, bool normalized, bool integer);
VkFormat getVkFormat(TextureFormat format);
VkFormat getVkFormat(PixelDataFormat format, PixelDataType type);
VkFormat getVkFormatLinear(VkFormat format);
uint32_t getBytesPerPixel(TextureFormat format);
VkCompareOp getCompareOp(SamplerCompareFunc func);
VkBlendFactor getBlendFactor(BlendFunction mode);
VkCullModeFlags getCullMode(CullingMode mode);
VkFrontFace getFrontFace(bool inverseFrontFaces);
PixelDataType getComponentType(VkFormat format);
VkComponentMapping getSwizzleMap(TextureSwizzle swizzle[4]);
size_t getElementTypeSize(ElementType type);
size_t getFormatSize(TextureFormat format);
void transitionImageLayout(VkCommandBuffer cmdbuffer, VulkanLayoutTransition transition);
bool equivalent(const VkRect2D& a, const VkRect2D& b);
bool equivalent(const VkExtent2D& a, const VkExtent2D& b);
bool operator<(const VkImageSubresourceRange& a, const VkImageSubresourceRange& b);

// bit mask

template<typename Enum>
struct EnableBitMaskOperators : public std::false_type { };

template<typename Enum, typename std::enable_if<
        std::is_enum<Enum>::value , int>::type = 0>
inline constexpr bool operator!(Enum rhs) noexcept {
    using underlying = typename std::underlying_type<Enum>::type;
    return underlying(rhs) == 0;
}

template<typename Enum, typename std::enable_if<
        std::is_enum<Enum>::value, int>::type = 0>
inline constexpr Enum operator~(Enum rhs) noexcept {
    using underlying = typename std::underlying_type<Enum>::type;
    return Enum(~underlying(rhs));
}

template<typename Enum, typename std::enable_if<
        std::is_enum<Enum>::value, int>::type = 0>
inline constexpr Enum operator|(Enum lhs, Enum rhs) noexcept {
    using underlying = typename std::underlying_type<Enum>::type;
    return Enum(underlying(lhs) | underlying(rhs));
}

template<typename Enum, typename std::enable_if<
        std::is_enum<Enum>::value, int>::type = 0>
inline constexpr Enum operator&(Enum lhs, Enum rhs) noexcept {
    using underlying = typename std::underlying_type<Enum>::type;
    return Enum(underlying(lhs) & underlying(rhs));
}

template<typename Enum, typename std::enable_if<
        std::is_enum<Enum>::value, int>::type = 0>
inline constexpr Enum operator^(Enum lhs, Enum rhs) noexcept {
    using underlying = typename std::underlying_type<Enum>::type;
    return Enum(underlying(lhs) ^ underlying(rhs));
}

template<typename Enum, typename std::enable_if<
        std::is_enum<Enum>::value, int>::type = 0>
inline constexpr Enum operator|=(Enum& lhs, Enum rhs) noexcept {
    return lhs = lhs | rhs;
}

template<typename Enum, typename std::enable_if<
        std::is_enum<Enum>::value, int>::type = 0>
inline constexpr Enum operator&=(Enum& lhs, Enum rhs) noexcept {
    return lhs = lhs & rhs;
}

template<typename Enum, typename std::enable_if<
        std::is_enum<Enum>::value, int>::type = 0>
inline constexpr Enum operator^=(Enum& lhs, Enum rhs) noexcept {
    return lhs = lhs ^ rhs;
}

template<typename Enum, typename std::enable_if<
        std::is_enum<Enum>::value, int>::type = 0>
inline constexpr bool none(Enum lhs) noexcept {
    return !lhs;
}

template<typename Enum, typename std::enable_if<
        std::is_enum<Enum>::value, int>::type = 0>
inline constexpr bool any(Enum lhs) noexcept {
    return !none(lhs);
}

} // namespace backend
} // namespace VR

#endif // VULKAN_UTILITY_H
