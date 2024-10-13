#ifndef VRDefine_h
#define VRDefine_h

#include <assert.h>
#include <stdio.h>

#if defined(__APPLE__)
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE
#define VR_BUILD_FOR_IOS
#endif
#endif

#ifdef VR_USE_LOGCAT
#include <android/log.h>
#define VR_ERROR(format, ...) __android_log_print(ANDROID_LOG_ERROR, "VRJNI", format, ##__VA_ARGS__)
#define VR_PRINT(format, ...) __android_log_print(ANDROID_LOG_INFO, "VRJNI", format, ##__VA_ARGS__)
#else
#define VR_PRINT(format, ...) printf(format, ##__VA_ARGS__)
#define VR_ERROR(format, ...) printf(format, ##__VA_ARGS__)
#endif

#define VR_PRINT_WITH_FUNC(format, ...) printf(format", FILE: %s, LINE: %d\n", ##__VA_ARGS__, __FILE__, __LINE__)
#define VR_ERROR_WITH_FUNC(format, ...) printf(format", FILE: %s, LINE: %d\n", ##__VA_ARGS__, __FILE__, __LINE__)

#ifdef VR_RUNTIME_DEBUG
#define VR_ASSERT(x)                                            \
    {                                                            \
        int res = (x);                                           \
        if (!res) {                                              \
            VR_ERROR("Error for %s, %d\n", __FILE__, __LINE__); \
            assert(res);                                         \
        }                                                        \
    }
#else
#define VR_ASSERT(x)
#endif

#define VR_FUNC_PRINT(x) VR_PRINT(#x "=%d in %s, %d \n", x, __func__, __LINE__);
#define VR_FUNC_PRINT_ALL(x, type) VR_PRINT(#x "=" #type " %" #type " in %s, %d \n", x, __func__, __LINE__);

#define VR_CHECK(success, log) \
if(!(success)){ \
VR_ERROR("Check failed: %s ==> %s\n", #success, #log); \
}

#if defined(_MSC_VER)
#if defined(BUILDING_VR_DLL)
#define VR_PUBLIC __declspec(dllexport)
#elif defined(USING_VR_DLL)
#define VR_PUBLIC __declspec(dllimport)
#else
#define VR_PUBLIC
#endif
#else
#define VR_PUBLIC __attribute__((visibility("default")))
#endif

#endif /* VRDefine_h */
