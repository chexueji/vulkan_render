#include "stdio.h"
#include "stdlib.h"
#include <vector>
#include "../VulkanWrapper.h"

static VkInstance& CreateInstance()
{
    VkApplicationInfo appInfo = {};
    VkInstanceCreateInfo instanceCreateInfo = {};

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "vulkan test";
    appInfo.applicationVersion = 1;
    appInfo.apiVersion = VK_API_VERSION_1_0;

    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceCreateInfo.pApplicationInfo = &appInfo;
    instanceCreateInfo.enabledExtensionCount = 0;
    instanceCreateInfo.ppEnabledExtensionNames = nullptr;

    VkInstance instance;
    VkResult result = vkCreateInstance(&instanceCreateInfo, nullptr, &instance);

    if (result != VK_SUCCESS) {
        instance = NULL;
        printf("Vulkan create instance failed!\n");
    }

    return instance;
}

static void EnumeratePhysicalDevices(VkInstance instance)
{
    if (instance == VK_NULL_HANDLE) {
        printf("No vulkan instance!\n");
        return;
    } 

    uint32_t physicalDeviceCount = 0;
    std::vector<VkPhysicalDevice> physicalDevices;

    VkResult result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
    if (result != VK_SUCCESS) {
        printf("Enumerate physical devices failed!\n");
        return;
    }
    if (physicalDeviceCount == 0) {
        printf("No physical devices!\n");
        return;
    }
    physicalDevices.resize(physicalDeviceCount);

    result = vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, &physicalDevices[0]);
    if (result != VK_SUCCESS) {
        printf("Enumerate physical devices success!\n");
        return;
    } else {
        printf("Enumerate physical devices failed!\n");
        return;
    }
}

static void DestroyInstance(VkInstance instance)
{
    if (instance != VK_NULL_HANDLE) {
        printf("Vulkan instance is destroyed!\n");
        vkDestroyInstance(instance, nullptr);
    }
}

int main()
{
    bool inited = InitVulkan();
    if (!inited) {
        printf("Vulkan initialize failed!\n");
        return -1;
    }

    VkInstance instance = CreateInstance();

    if (instance != VK_NULL_HANDLE) {
        printf("Vulkan bind instance %lld functions success!\n", instance);
        BindVulkan(instance);
    }

    uint32_t version = GetInstanceVersion();
    printf("Vulkan version %d.%d.%d initialized.\n", VK_VERSION_MAJOR(version), VK_VERSION_MINOR(version), VK_VERSION_PATCH(version));

    EnumeratePhysicalDevices(instance);

    DestroyInstance(instance);

    return 0;
}