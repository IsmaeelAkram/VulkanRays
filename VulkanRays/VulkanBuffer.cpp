#include "VulkanBuffer.h"
#include <stdexcept>
#include <cstring>

VulkanBuffer::VulkanBuffer(VulkanDevice& device, VkPhysicalDevice physicalDevice, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
    : device(device.getDevice()) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device.getDevice(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create buffer");
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device.getDevice(), buffer, &memRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    // Find memory type index (user must provide helper in VulkanDevice)
    allocInfo.memoryTypeIndex = device.findMemoryType(memRequirements.memoryTypeBits, properties); // Set correct memory type index
    if (vkAllocateMemory(device.getDevice(), &allocInfo, nullptr, &memory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate buffer memory");
    vkBindBufferMemory(device.getDevice(), buffer, memory, 0);
}

VulkanBuffer::~VulkanBuffer() {
    destroy();
}

void VulkanBuffer::destroy() {
    if (buffer) {
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (memory) {
        vkFreeMemory(device, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
}

void VulkanBuffer::uploadData(const void* src, VkDeviceSize size) {
    void* data;
    vkMapMemory(device, memory, 0, size, 0, &data);
    std::memcpy(data, src, (size_t)size);
    vkUnmapMemory(device, memory);
}
