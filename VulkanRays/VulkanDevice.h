#pragma once
#include <vulkan/vulkan.h>

class VulkanDevice {
public:
    VulkanDevice(VkInstance instance, VkSurfaceKHR surface);
    ~VulkanDevice();
    VkDevice getDevice() const;
    VkPhysicalDevice getPhysicalDevice() const;
    VkQueue getGraphicsQueue() const;
    VkQueue getPresentQueue() const;
    uint32_t getGraphicsQueueFamily() const;
    uint32_t getPresentQueueFamily() const;
    // Utility for memory type selection
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
private:
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue presentQueue = VK_NULL_HANDLE;
    uint32_t graphicsQueueFamily = 0;
    uint32_t presentQueueFamily = 0;
    // ...other members...
};
