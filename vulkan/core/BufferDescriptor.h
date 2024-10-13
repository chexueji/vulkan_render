#ifndef BUFFER_DESCRIPTOR_H
#define BUFFER_DESCRIPTOR_H

#include "VulkanEnums.h"
#include "VulkanMacros.h"

namespace VR {
namespace backend {

class  BufferDescriptor {
public:

    // callback to destroy the buffer data
    using Callback = void(*)(void* buffer, size_t size, void* user);

    BufferDescriptor()  = default;
    ~BufferDescriptor()  {
        if (callback) {
            callback(buffer, size, user);
        }
    }

    BufferDescriptor(const BufferDescriptor& rhs) = delete;
    BufferDescriptor& operator=(const BufferDescriptor& rhs) = delete;

    BufferDescriptor(BufferDescriptor&& rhs) : buffer(rhs.buffer), size(rhs.size), callback(rhs.callback), user(rhs.user) {
            rhs.buffer = nullptr;
            rhs.callback = nullptr;
    }

    BufferDescriptor& operator=(BufferDescriptor&& rhs)  {
        if (this != &rhs) {
            buffer = rhs.buffer;
            size = rhs.size;
            callback = rhs.callback;
            user = rhs.user;
            rhs.buffer = nullptr;
            rhs.callback = nullptr;
        }
        return *this;
    }

    BufferDescriptor(void const* buffer, size_t size, Callback callback = nullptr, void* user = nullptr) 
                     : buffer(const_cast<void*>(buffer)), size(size), callback(callback), user(user) {
    }

    void setCallback(Callback callback, void* user = nullptr)  {
        this->callback = callback;
        this->user = user;
    }

    bool hasCallback() const  { return callback != nullptr; }

    Callback getCallback() const  {
        return callback;
    }

    void* getUser() const  {
        return user;
    }

    void* buffer = nullptr;
    size_t size = 0;

private:
    Callback callback = nullptr;
    void* user = nullptr;
};

class PixelBufferDescriptor : public BufferDescriptor {
public:

    PixelBufferDescriptor() = default;

    PixelBufferDescriptor(void const* buffer, size_t size,
            PixelDataFormat format, PixelDataType type, uint8_t alignment = 1,
            uint32_t left = 0, uint32_t top = 0, uint32_t stride = 0,
            Callback callback = nullptr, void* user = nullptr) 
            : BufferDescriptor(buffer, size, callback, user),
              left(left), top(top), stride(stride),
              format(format), type(type), alignment(alignment) {
    }

    PixelBufferDescriptor(void const* buffer, size_t size,
            PixelDataFormat format, PixelDataType type,
            Callback callback, void* user = nullptr) 
            : BufferDescriptor(buffer, size, callback, user),
              stride(0), format(format), type(type), alignment(1) {
    }

    PixelBufferDescriptor(void const* buffer, size_t size,
            CompressedPixelDataType format, uint32_t imageSize,
            Callback callback, void* user = nullptr) 
            : BufferDescriptor(buffer, size, callback, user),
              imageSize(imageSize), compressedFormat(format), type(PixelDataType::COMPRESSED),
              alignment(1) {
    }

    static constexpr size_t computeDataSize(PixelDataFormat format, PixelDataType type, size_t stride, size_t height, size_t alignment)  {
        
        VR_ASSERT(alignment);

        if (type == PixelDataType::COMPRESSED) {
            return 0;
        }

        size_t n = 0;
        switch (format) {
            case PixelDataFormat::R:
            case PixelDataFormat::R_INTEGER:
            case PixelDataFormat::DEPTH_COMPONENT:
            case PixelDataFormat::ALPHA:
                n = 1;
                break;
            case PixelDataFormat::RG:
            case PixelDataFormat::RG_INTEGER:
            case PixelDataFormat::DEPTH_STENCIL:
                n = 2;
                break;
            case PixelDataFormat::RGB:
            case PixelDataFormat::RGB_INTEGER:
                n = 3;
                break;
            case PixelDataFormat::UNUSED:
            case PixelDataFormat::RGBA:
            case PixelDataFormat::RGBA_INTEGER:
                n = 4;
                break;
        }

        size_t bpp = n;
        switch (type) {
            case PixelDataType::COMPRESSED:
            case PixelDataType::UBYTE:
            case PixelDataType::BYTE:
                // nothing to do
                break;
            case PixelDataType::USHORT:
            case PixelDataType::SHORT:
            case PixelDataType::HALF:
                bpp *= 2;
                break;
            case PixelDataType::UINT:
            case PixelDataType::INT:
            case PixelDataType::FLOAT:
                bpp *= 4;
                break;
            case PixelDataType::UINT_10F_11F_11F_REV:
                VR_ASSERT(format == PixelDataFormat::RGB);
                bpp = 4;
                break;
            case PixelDataType::UINT_2_10_10_10_REV:
                VR_ASSERT(format == PixelDataFormat::RGBA);
                bpp = 4;
                break;
            case PixelDataType::USHORT_565:
                VR_ASSERT(format == PixelDataFormat::RGB);
                bpp = 2;
                break;
        }

        size_t bpr = bpp * stride;
        size_t bprAligned = (bpr + (alignment - 1)) & -alignment;
        return bprAligned * height;
    }
  
    uint32_t left = 0;
    uint32_t top = 0;
    union {
        struct {         
            uint32_t stride;          
            PixelDataFormat format;
        };
        struct {        
            uint32_t imageSize;
            CompressedPixelDataType compressedFormat;
        };
    };
    PixelDataType type;
    uint8_t alignment;
};

} // namespace backend
} // namespace VR

#endif // BUFFER_DESCRIPTOR_H
