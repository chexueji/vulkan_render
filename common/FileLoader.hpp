#ifndef FileLoader_hpp
#define FileLoader_hpp
#include <VRDefine.h>
#include "RefCount.h"
namespace VR {
class VR_PUBLIC RawBuffer : public RefCount {
public:
    RawBuffer(size_t size);
    virtual ~ RawBuffer();
    bool valid() const {
        return nullptr != mBuffer;
    }
    inline size_t size() const {
        return mTotalSize;
    }
    void* buffer() {
        return mBuffer;
    }
private:
    void* mBuffer;
    size_t mTotalSize           = 0;
};
class VR_PUBLIC FileLoader : public RefCount {
public:

    FileLoader(const char* file);

    ~FileLoader();

    SharedPtr<RawBuffer> read();
    SharedPtr<RawBuffer> read(size_t offset, size_t length);

    bool valid() const {
        return mFile != nullptr;
    }
private:
    FILE* mFile                 = nullptr;
    static const int gCacheSize = 4096;
    const char* mFilePath       = nullptr;
};
}
#endif
