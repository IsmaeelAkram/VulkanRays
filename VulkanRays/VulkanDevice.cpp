#include "VulkanDevice.h"
#include <vector>
#include <set>
#include <stdexcept>

namespace {
bool isDeviceSuitable(VkPhysicalDevice dev, VkSurfaceKHR surface, uint32_t& graphicsIdx, uint32_t& presentIdx) {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(dev, &queueFamilyCount, families.data());
    bool foundGraphics = false, foundPresent = false;
    for (uint32_t i = 0; i < queueFamilyCount; ++i) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsIdx = i;
            foundGraphics = true;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, surface, &presentSupport);
        if (presentSupport) {
            presentIdx = i;
            foundPresent = true;
        }
        if (foundGraphics && foundPresent) break;
    }
    return foundGraphics && foundPresent;
}
}

VulkanDevice::VulkanDevice(VkInstance instance, VkSurfaceKHR surface) {
    // Pick physical device
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (deviceCount == 0) throw std::runtime_error("No Vulkan devices found");
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    for (const auto& dev : devices) {
        uint32_t gIdx = 0, pIdx = 0;
        if (isDeviceSuitable(dev, surface, gIdx, pIdx)) {
            physicalDevice = dev;
            graphicsQueueFamily = gIdx;
            presentQueueFamily = pIdx;
            break;
        }
    }
    if (physicalDevice == VK_NULL_HANDLE) throw std::runtime_error("No suitable Vulkan device found");
    // Create logical device
    std::set<uint32_t> uniqueFamilies = { graphicsQueueFamily, presentQueueFamily };
    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    for (uint32_t family : uniqueFamilies) {
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = family;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueInfo);
    }
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();
    if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS)
        throw std::runtime_error("Failed to create logical device");
    vkGetDeviceQueue(device, graphicsQueueFamily, 0, &graphicsQueue);
    vkGetDeviceQueue(device, presentQueueFamily, 0, &presentQueue);
}

VulkanDevice::~VulkanDevice() {
    if (device) {
        vkDestroyDevice(device, nullptr);
    }
}

VkDevice VulkanDevice::getDevice() const { return device; }
VkPhysicalDevice VulkanDevice::getPhysicalDevice() const { return physicalDevice; }
VkQueue VulkanDevice::getGraphicsQueue() const { return graphicsQueue; }
VkQueue VulkanDevice::getPresentQueue() const { return presentQueue; }
uint32_t VulkanDevice::getGraphicsQueueFamily() const { return graphicsQueueFamily; }
uint32_t VulkanDevice::getPresentQueueFamily() const { return presentQueueFamily; }
uint32_t VulkanDevice::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    throw std::runtime_error("Failed to find suitable memory type");
}
