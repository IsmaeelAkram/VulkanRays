#pragma once
#include "VulkanDevice.h"
#include <vulkan/vulkan.h>

class VulkanBuffer {
public:
    VulkanBuffer(VulkanDevice& device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
    ~VulkanBuffer();
    VkBuffer getBuffer() const { return buffer; }
    VkDeviceMemory getMemory() const { return memory; }
    void uploadData(const void* src, VkDeviceSize size);
private:
    VkDevice device;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
};
