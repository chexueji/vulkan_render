#ifndef VULKAN_ENUMS_H
#define VULKAN_ENUMS_H

#include <array>
#include <vector>   
#include <stddef.h>
#include <stdint.h>
#include "VulkanMacros.h"

namespace VR {
namespace backend {

enum class TargetBufferFlags : uint32_t {
    NONE = 0x0u, 
    COLOR0 = 0x00000001u,
    COLOR1 = 0x00000002u,
    COLOR2 = 0x00000004u,
    COLOR3 = 0x00000008u,
    COLOR4 = 0x00000010u,
    COLOR5 = 0x00000020u,
    COLOR6 = 0x00000040u,
    COLOR7 = 0x00000080u,

    COLOR = COLOR0,                         
    COLOR_ALL = COLOR0 | COLOR1 | COLOR2 | COLOR3 | COLOR4 | COLOR5 | COLOR6 | COLOR7,
    DEPTH   = 0x10000000u,                  
    STENCIL = 0x20000000u,                  
    DEPTH_AND_STENCIL = DEPTH | STENCIL,
    ALL = COLOR_ALL | DEPTH | STENCIL 
};

inline TargetBufferFlags getTargetBufferFlagsAt(size_t index) {
    if (index == 0u) return TargetBufferFlags::COLOR0;
    if (index == 1u) return TargetBufferFlags::COLOR1;
    if (index == 2u) return TargetBufferFlags::COLOR2;
    if (index == 3u) return TargetBufferFlags::COLOR3;
    if (index == 4u) return TargetBufferFlags::COLOR4;
    if (index == 5u) return TargetBufferFlags::COLOR5;
    if (index == 6u) return TargetBufferFlags::COLOR6;
    if (index == 7u) return TargetBufferFlags::COLOR7;
    if (index == 8u) return TargetBufferFlags::DEPTH;
    if (index == 9u) return TargetBufferFlags::STENCIL;
    return TargetBufferFlags::NONE;
}

enum class BufferUsage : uint8_t {
    STATIC,      
    DYNAMIC,     
    STREAM, 
};

struct Viewport {
    int32_t left;       
    int32_t bottom;     
    uint32_t width;     
    uint32_t height;    
    int32_t right() const { return left + width; }
    int32_t top() const { return bottom + height; }
};

using Scissor = Viewport;

struct DepthRange {
    float near = 0.0f;   
    float far = 1.0f;
    DepthRange(float n, float f):near(n), far(f) {}
};

enum class ShaderModel : uint8_t {
    UNKNOWN    = 0,
    GL_ES_30   = 1,   
    GL_CORE_41 = 2,    
};

enum class PrimitiveType : uint8_t {
    POINTS          = 0,    
    LINES           = 1,    
    TRIANGLES       = 4,
    TRIANGLE_STRIP  = 5,    
    NONE            = 0xFF
};

enum class UniformType : uint8_t {
    BOOL,
    BOOL2,
    BOOL3,
    BOOL4,
    FLOAT,
    FLOAT2,
    FLOAT3,
    FLOAT4,
    INT,
    INT2,
    INT3,
    INT4,
    UINT,
    UINT2,
    UINT3,
    UINT4,
    MAT3,   
    MAT4  
};

enum class Precision : uint8_t {
    LOW,
    MEDIUM,
    HIGH,
    DEFAULT
};

enum class SamplerType : uint8_t {
    SAMPLER_2D,         
    SAMPLER_2D_ARRAY,   
    SAMPLER_CUBEMAP,    
    SAMPLER_EXTERNAL,   
    SAMPLER_3D,         
};

enum class SubpassType : uint8_t {
    SUBPASS_INPUT
};

enum class SamplerFormat : uint8_t {
    INT = 0,        
    UINT = 1,       
    FLOAT = 2,      
    SHADOW = 3   
};

enum class ElementType : uint8_t {
    BYTE,
    BYTE2,
    BYTE3,
    BYTE4,
    UBYTE,
    UBYTE2,
    UBYTE3,
    UBYTE4,
    SHORT,
    SHORT2,
    SHORT3,
    SHORT4,
    USHORT,
    USHORT2,
    USHORT3,
    USHORT4,
    INT,
    INT2,
    INT3,
    INT4,
    UINT,
    UINT2,
    UINT3,
    UINT4,
    FLOAT,
    FLOAT2,
    FLOAT3,
    FLOAT4,
    HALF,
    HALF2,
    HALF3,
    HALF4,
    UNKNOWN,
};

enum class BufferObjectBinding : uint8_t {
    VERTEX,
};

enum class CullingMode : uint8_t {
    NONE,               
    FRONT,              
    BACK,               
    FRONT_AND_BACK      
};

enum class PixelDataFormat : uint8_t {
    R,                
    R_INTEGER,        
    RG,               
    RG_INTEGER,       
    RGB,              
    RGB_INTEGER,      
    RGBA,             
    RGBA_INTEGER,     
    UNUSED,           
    DEPTH_COMPONENT,  
    DEPTH_STENCIL,    
    ALPHA             
};

enum class PixelDataType : uint8_t {
    UBYTE,                
    BYTE,                 
    USHORT,               
    SHORT,                
    UINT,                 
    INT,                  
    HALF,                 
    FLOAT,                
    COMPRESSED,           
    UINT_10F_11F_11F_REV, 
    USHORT_565,           
    UINT_2_10_10_10_REV,  
};


enum class CompressedPixelDataType : uint16_t {
    EAC_R11, EAC_R11_SIGNED, EAC_RG11, EAC_RG11_SIGNED,
    ETC2_RGB8, ETC2_SRGB8,
    ETC2_RGB8_A1, ETC2_SRGB8_A1,
    ETC2_EAC_RGBA8, ETC2_EAC_SRGBA8,

    // except Android/iOS
    DXT1_RGB, DXT1_RGBA, DXT3_RGBA, DXT5_RGBA,
    DXT1_SRGB, DXT1_SRGBA, DXT3_SRGBA, DXT5_SRGBA,

    // ASTC formats
    RGBA_ASTC_4x4,
    RGBA_ASTC_5x4,
    RGBA_ASTC_5x5,
    RGBA_ASTC_6x5,
    RGBA_ASTC_6x6,
    RGBA_ASTC_8x5,
    RGBA_ASTC_8x6,
    RGBA_ASTC_8x8,
    RGBA_ASTC_10x5,
    RGBA_ASTC_10x6,
    RGBA_ASTC_10x8,
    RGBA_ASTC_10x10,
    RGBA_ASTC_12x10,
    RGBA_ASTC_12x12,
    SRGB8_ALPHA8_ASTC_4x4,
    SRGB8_ALPHA8_ASTC_5x4,
    SRGB8_ALPHA8_ASTC_5x5,
    SRGB8_ALPHA8_ASTC_6x5,
    SRGB8_ALPHA8_ASTC_6x6,
    SRGB8_ALPHA8_ASTC_8x5,
    SRGB8_ALPHA8_ASTC_8x6,
    SRGB8_ALPHA8_ASTC_8x8,
    SRGB8_ALPHA8_ASTC_10x5,
    SRGB8_ALPHA8_ASTC_10x6,
    SRGB8_ALPHA8_ASTC_10x8,
    SRGB8_ALPHA8_ASTC_10x10,
    SRGB8_ALPHA8_ASTC_12x10,
    SRGB8_ALPHA8_ASTC_12x12,
};

enum class TextureFormat : uint16_t {
    // 8-bits per element
    R8, R8_SNORM, R8UI, R8I, STENCIL8,

    // 16-bits per element
    R16F, R16UI, R16I,
    RG8, RG8_SNORM, RG8UI, RG8I,
    RGB565,
    RGB9_E5, 
    RGB5_A1,
    RGBA4,
    DEPTH16,

    // 24-bits per element
    RGB8, SRGB8, RGB8_SNORM, RGB8UI, RGB8I,
    DEPTH24,

    // 32-bits per element
    R32F, R32UI, R32I,
    RG16F, RG16UI, RG16I,
    R11F_G11F_B10F,
    RGBA8, SRGB8_A8,RGBA8_SNORM,
    UNUSED, // used to be rgbm
    RGB10_A2, RGBA8UI, RGBA8I,
    DEPTH32F, DEPTH24_STENCIL8, DEPTH32F_STENCIL8,

    // 48-bits per element
    RGB16F, RGB16UI, RGB16I,

    // 64-bits per element
    RG32F, RG32UI, RG32I,
    RGBA16F, RGBA16UI, RGBA16I,

    // 96-bits per element
    RGB32F, RGB32UI, RGB32I,

    // 128-bits per element
    RGBA32F, RGBA32UI, RGBA32I,

    // compressed formats

    // GLES 3.0 and GL 4.3
    EAC_R11, EAC_R11_SIGNED, EAC_RG11, EAC_RG11_SIGNED,
    ETC2_RGB8, ETC2_SRGB8,
    ETC2_RGB8_A1, ETC2_SRGB8_A1,
    ETC2_EAC_RGBA8, ETC2_EAC_SRGBA8,

    // except Android/iOS
    DXT1_RGB, DXT1_RGBA, DXT3_RGBA, DXT5_RGBA,
    DXT1_SRGB, DXT1_SRGBA, DXT3_SRGBA, DXT5_SRGBA,

    // ASTC formats
    RGBA_ASTC_4x4,
    RGBA_ASTC_5x4,
    RGBA_ASTC_5x5,
    RGBA_ASTC_6x5,
    RGBA_ASTC_6x6,
    RGBA_ASTC_8x5,
    RGBA_ASTC_8x6,
    RGBA_ASTC_8x8,
    RGBA_ASTC_10x5,
    RGBA_ASTC_10x6,
    RGBA_ASTC_10x8,
    RGBA_ASTC_10x10,
    RGBA_ASTC_12x10,
    RGBA_ASTC_12x12,
    SRGB8_ALPHA8_ASTC_4x4,
    SRGB8_ALPHA8_ASTC_5x4,
    SRGB8_ALPHA8_ASTC_5x5,
    SRGB8_ALPHA8_ASTC_6x5,
    SRGB8_ALPHA8_ASTC_6x6,
    SRGB8_ALPHA8_ASTC_8x5,
    SRGB8_ALPHA8_ASTC_8x6,
    SRGB8_ALPHA8_ASTC_8x8,
    SRGB8_ALPHA8_ASTC_10x5,
    SRGB8_ALPHA8_ASTC_10x6,
    SRGB8_ALPHA8_ASTC_10x8,
    SRGB8_ALPHA8_ASTC_10x10,
    SRGB8_ALPHA8_ASTC_12x10,
    SRGB8_ALPHA8_ASTC_12x12,
};

enum class TextureUsage : uint8_t {
    NONE                = 0x0,
    COLOR_ATTACHMENT    = 0x1,                      
    DEPTH_ATTACHMENT    = 0x2,                      
    STENCIL_ATTACHMENT  = 0x4,                      
    UPLOADABLE          = 0x8,                      
    SAMPLEABLE          = 0x10,                     
    SUBPASS_INPUT       = 0x20,                     
    DEFAULT             = UPLOADABLE | SAMPLEABLE   
};

enum class TextureSwizzle : uint8_t {
    SUBSTITUTE_ZERO,
    SUBSTITUTE_ONE,
    CHANNEL_0,
    CHANNEL_1,
    CHANNEL_2,
    CHANNEL_3
};

static constexpr bool isDepthFormat(TextureFormat format) {
    switch (format) {
        case TextureFormat::DEPTH32F:
        case TextureFormat::DEPTH24:
        case TextureFormat::DEPTH16:
        case TextureFormat::DEPTH32F_STENCIL8:
        case TextureFormat::DEPTH24_STENCIL8:
            return true;
        default:
            return false;
    }
}

static constexpr bool isCompressedFormat(TextureFormat format) {
    return format >= TextureFormat::EAC_R11;
}

static constexpr bool isETC2Compression(TextureFormat format) {
    return format >= TextureFormat::EAC_R11 && format <= TextureFormat::ETC2_EAC_SRGBA8;
}

static constexpr bool isS3TCCompression(TextureFormat format) {
    return format >= TextureFormat::DXT1_RGB && format <= TextureFormat::DXT5_SRGBA;
}

static constexpr bool isS3TCSRGBCompression(TextureFormat format) {
    return format >= TextureFormat::DXT1_SRGB && format <= TextureFormat::DXT5_SRGBA;
}

enum class TextureCubemapFace : uint8_t {
    POSITIVE_X = 0, 
    NEGATIVE_X = 1, 
    POSITIVE_Y = 2, 
    NEGATIVE_Y = 3, 
    POSITIVE_Z = 4, 
    NEGATIVE_Z = 5, 
};

struct FaceOffsets {
    using size_type = size_t;
    union {
        struct {
            size_type px;
            size_type nx;
            size_type py;
            size_type ny;
            size_type pz;
            size_type nz;
        };
        size_type offsets[6];
    };
    size_type  operator[](size_t n) const { return offsets[n]; }
    size_type& operator[](size_t n) { return offsets[n]; }
    FaceOffsets() = default;
    FaceOffsets(size_type faceSize) {
        px = faceSize * 0;
        nx = faceSize * 1;
        py = faceSize * 2;
        ny = faceSize * 3;
        pz = faceSize * 4;
        nz = faceSize * 5;
    }
    FaceOffsets(const FaceOffsets& rhs) {
        px = rhs.px;
        nx = rhs.nx;
        py = rhs.py;
        ny = rhs.ny;
        pz = rhs.pz;
        nz = rhs.nz;
    }
    FaceOffsets& operator=(const FaceOffsets& rhs) {
        px = rhs.px;
        nx = rhs.nx;
        py = rhs.py;
        ny = rhs.ny;
        pz = rhs.pz;
        nz = rhs.nz;
        return *this;
    }
};

enum class SamplerWrapMode : uint8_t {
    CLAMP_TO_EDGE,  
    REPEAT,         
    MIRRORED_REPEAT,
};

enum class SamplerMinFilter : uint8_t {

    NEAREST = 0,               
    LINEAR = 1,                
    NEAREST_MIPMAP_NEAREST = 2,
    LINEAR_MIPMAP_NEAREST = 3, 
    NEAREST_MIPMAP_LINEAR = 4, 
    LINEAR_MIPMAP_LINEAR = 5   
};

enum class SamplerMagFilter : uint8_t {
    NEAREST = 0,               
    LINEAR = 1,                
};

enum class SamplerCompareMode : uint8_t {
    NONE = 0,
    COMPARE_TO_TEXTURE = 1
};

enum class SamplerCompareFunc : uint8_t {

    LE = 0,   
    GE,       
    L,        
    G,        
    E,        
    NE,       
    A,        
    N         
};

struct SamplerParams { 
    union {
        struct {
            SamplerMagFilter filterMag      : 1;
            SamplerMinFilter filterMin      : 3;
            SamplerWrapMode wrapS           : 2;
            SamplerWrapMode wrapT           : 2;

            SamplerWrapMode wrapR           : 2;
            uint8_t anisotropyLog2          : 3;
            SamplerCompareMode compareMode  : 1;
            uint8_t padding0                : 2;

            SamplerCompareFunc compareFunc  : 3;
            uint8_t padding1                : 5;

            uint8_t padding2                : 8;
        };
        uint32_t u;
    };
private:
    friend inline bool operator < (SamplerParams lhs, SamplerParams rhs) {
        return lhs.u < rhs.u;
    }
};

enum class BlendEquation : uint8_t {
    ADD,                    
    SUBTRACT,               
    REVERSE_SUBTRACT,       
    MIN,                    
    MAX                     
};

enum class BlendFunction : uint8_t {
    ZERO,                  
    ONE,                   
    SRC_COLOR,             
    ONE_MINUS_SRC_COLOR,   
    DST_COLOR,             
    ONE_MINUS_DST_COLOR,   
    SRC_ALPHA,             
    ONE_MINUS_SRC_ALPHA,   
    DST_ALPHA,             
    ONE_MINUS_DST_ALPHA,   
    SRC_ALPHA_SATURATE     
};

struct Attribute {
    static constexpr uint8_t FLAG_NORMALIZED     = 0x1;
    static constexpr uint8_t FLAG_INTEGER_TARGET = 0x2;
    static constexpr uint8_t BUFFER_UNUSED = 0xFF;
    uint32_t offset = 0;                   
    uint8_t stride = 0;                    
    uint8_t buffer = BUFFER_UNUSED;        
    ElementType type = ElementType::UNKNOWN;  
    uint8_t flags = 0x0;                   
};

using AttributeArray = std::array<Attribute, MAX_VERTEX_ATTRIBUTE_COUNT>;

enum ShaderType : uint8_t {
    VERTEX = 0,
    FRAGMENT = 1
};
static constexpr size_t PIPELINE_STAGE_COUNT = 2;

// offset = (m * factor) + (r * units)
// m = max(dz/dx, dz/dy) r = minimal resolution of depth
struct PolygonOffset {
    float slope = 0;
    float constant = 0;
};

struct RasterStateT {
    // final color = src color * src factor (op) dst color * dst factor
    // src color: computed color in shader
    // dst color: color in framebuffer
    RasterStateT() {
        // default: disable alpha blend
        culling = CullingMode::BACK;
        blendEquationRGB = BlendEquation::ADD;
        blendEquationAlpha = BlendEquation::ADD;
        blendFunctionSrcRGB = BlendFunction::ONE;
        blendFunctionSrcAlpha = BlendFunction::ONE;
        blendFunctionDstRGB = BlendFunction::ZERO;
        blendFunctionDstAlpha = BlendFunction::ZERO;
    }

    bool operator == (RasterStateT rhs) const { return u == rhs.u; }
    bool operator != (RasterStateT rhs) const { return u != rhs.u; }

    void disableBlending() {
        blendEquationRGB = BlendEquation::ADD;
        blendEquationAlpha = BlendEquation::ADD;
        blendFunctionSrcRGB = BlendFunction::ONE;
        blendFunctionSrcAlpha = BlendFunction::ONE;
        blendFunctionDstRGB = BlendFunction::ZERO;
        blendFunctionDstAlpha = BlendFunction::ZERO;
    }

    bool hasBlending() const {
        return !(blendEquationRGB == BlendEquation::ADD &&
                 blendEquationAlpha == BlendEquation::ADD &&
                 blendFunctionSrcRGB == BlendFunction::ONE &&
                 blendFunctionSrcAlpha == BlendFunction::ONE &&
                 blendFunctionDstRGB == BlendFunction::ZERO &&
                 blendFunctionDstAlpha == BlendFunction::ZERO);
    }

    union {
        struct {

            CullingMode culling; 
            BlendEquation blendEquationRGB;
            BlendEquation blendEquationAlpha;
            BlendFunction blendFunctionSrcRGB;
            BlendFunction blendFunctionSrcAlpha;
            BlendFunction blendFunctionDstRGB;
            BlendFunction blendFunctionDstAlpha;
            bool depthWrite;
            SamplerCompareFunc depthFunc;
            bool colorWrite;     
            bool alphaToCoverage;     
            bool inverseFrontFaces;
        };
        uint32_t u = 0;
    };
};

struct RenderPassFlags {
    TargetBufferFlags clear;
    TargetBufferFlags discardStart; // for multi passes
    TargetBufferFlags discardEnd; // for multi passes
};

struct RenderPassParams {
    RenderPassFlags flags;
    Viewport viewport = {0, 0, 0, 0};
    Scissor scissor = {0, 0, 0, 0};
    DepthRange depthRange = {0.0, 1.0};
    std::array<float, 4> clearColor = {0.0, 0.0, 0.0, 0.0};
    double clearDepth = 1.0;
    uint32_t clearStencil = 0;
    uint32_t subpassMask = 0;
};

} // namespace backend
} // namespace VR

#endif // VULKAN_ENUMS_H
