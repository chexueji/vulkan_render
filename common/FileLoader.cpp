#include <stdlib.h>
#include "FileLoader.hpp"
#if defined(_MSC_VER)
#include "Windows.h"
#endif
namespace VR {
RawBuffer::RawBuffer(size_t size) {
    mBuffer = malloc(size);
    mTotalSize = size;
}
RawBuffer::~RawBuffer() {
    if (nullptr != mBuffer) {
        free(mBuffer);
    }
}

FileLoader::FileLoader(const char* file) {
#if defined(_MSC_VER)
    wchar_t wFilename[1024];
    if (0 == MultiByteToWideChar(CP_ACP, 0, file, -1, wFilename, sizeof(wFilename))) {
        mFile = nullptr;
        return;
    }
#if _MSC_VER >= 1400
    if (0 != _wfopen_s(&mFile, wFilename, L"rb")) {
        mFile = nullptr;
        return;
    }
#else
    mFile = _wfopen(wFilename, L"rb");
#endif
#else
    mFile = fopen(file, "rb");
#endif
    mFilePath = file;
    if (nullptr == mFile) {
        VR_ERROR("Open %s file Error\n", mFilePath);
    }
}

FileLoader::~FileLoader() {
    if (nullptr != mFile) {
        fclose(mFile);
    }
}
SharedPtr<RawBuffer> FileLoader::read(size_t offset, size_t length) {
    if (nullptr == mFile) {
        return nullptr;
    }
    if (0 == length && 0 == offset) {
        fseek(mFile, 0, SEEK_END);
        length = ftell(mFile);
    }

    if (0 == length) {
        return nullptr;
    }
    SharedPtr<RawBuffer> buffer = new RawBuffer(length);
    if (!buffer->valid()) {
        VR_ERROR("Memory Alloc Failed\n");
        return nullptr;
    }
    fseek(mFile, offset, SEEK_SET);
    auto block = (char*)buffer->buffer();
    auto size  = fread(block, 1, gCacheSize, mFile);
    auto currentSize = size;
    block += size;
    while (size == gCacheSize) {
        if (gCacheSize > length - currentSize)
        {
            int remaining = length - currentSize;
            size = fread(block, 1, remaining, mFile);
        }
        else
        {
            size = fread(block, 1, gCacheSize, mFile);
        }
        if (size > gCacheSize) {
            VR_PRINT("Read %s file Error\n", mFilePath);
            return nullptr;
        }
        currentSize += size;
        block+=size;
    }
    int err = ferror(mFile);
    if (err) {
        VR_ERROR("Read file error in VR Fileload\n");
    }
    return buffer;
}

SharedPtr<RawBuffer> FileLoader::read() {
    return read(0, 0);
}

} // namespace VR
