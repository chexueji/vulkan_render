#ifndef VRVULKAN_H
#define VRVULKAN_H
#include "VRDefine.h"

#define DEBUG 1
#ifdef DEBUG
#define VR_VK_ASSERT(x,log)                                                    \
    {                                                                           \
        int res = (x);                                                          \
        if (!res) {                                                             \
            VR_ERROR("Error for %s, line %d ==> %s\n", __FILE__, __LINE__, #log);   \
            assert(res);                                                        \
        }                                                                       \
    }
#else
#define VR_ASSERT(x)
#endif

 struct VRContextVulkanInfo {
     void* swapchain;
 };


// struct VRTargetVulkanInfo {

// };
#ifdef __cplusplus
extern "C" {
#endif

void VRVulkanContextSetInfo(struct VRContext* dst, struct VRContextVulkanInfo* info);

#ifdef __cplusplus
}
#endif

#endif // VRVULKAN_H
