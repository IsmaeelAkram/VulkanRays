#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <cstdint>

// --- CoreRendering: Depth Buffering ---
struct DepthResources {
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView view = VK_NULL_HANDLE;
    VkFormat format = VK_FORMAT_UNDEFINED;
};

void CreateDepthResources(VkDevice device, VkPhysicalDevice physicalDevice, VkExtent2D extent, VkFormat format, DepthResources& out, VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
void DestroyDepthResources(VkDevice device, DepthResources& res);
VkFormat FindSupportedDepthFormat(VkPhysicalDevice physicalDevice);

// --- CoreRendering: Swapchain Recreation ---
struct SwapchainResources {
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    VkFormat imageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D extent = {0,0};
};

void RecreateSwapchain(
    VkDevice device,
    VkPhysicalDevice physicalDevice,
    VkSurfaceKHR surface,
    VkInstance instance,
    VkExtent2D& windowExtent,
    SwapchainResources& swapchainRes,
    std::vector<VkFramebuffer>& framebuffers,
    VkRenderPass renderPass,
    DepthResources& depthRes
);

// --- CoreRendering: Double/Triple Buffering ---
struct SyncObjects {
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
};

void CreateSyncObjects(VkDevice device, int maxFramesInFlight, SyncObjects& sync);
void DestroySyncObjects(VkDevice device, SyncObjects& sync);

// --- CoreRendering: Descriptor Management ---
struct DescriptorPools {
    VkDescriptorPool uboPool = VK_NULL_HANDLE;
    VkDescriptorPool samplerPool = VK_NULL_HANDLE;
};

void CreateUBODescriptorPool(VkDevice device, uint32_t numSets, VkDescriptorPool& pool);
void CreateSamplerDescriptorPool(VkDevice device, uint32_t numSets, VkDescriptorPool& pool);
void DestroyDescriptorPools(VkDevice device, DescriptorPools& pools);
