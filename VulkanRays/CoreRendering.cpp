#include "CoreRendering.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <algorithm>
#include <stdexcept>

// --- CoreRendering: Depth Buffering ---
VkFormat FindSupportedDepthFormat(VkPhysicalDevice physicalDevice) {
    const VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            return format;
    }
    throw std::runtime_error("Failed to find supported depth format");
}

void CreateDepthResources(VkDevice device, VkPhysicalDevice physicalDevice, VkExtent2D extent, VkFormat format, DepthResources& out, VkImageUsageFlags usage) {
    out.format = format;
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = extent.width;
    imageInfo.extent.height = extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(device, &imageInfo, nullptr, &out.image) != VK_SUCCESS)
        throw std::runtime_error("Failed to create depth image");
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, out.image, &memRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    // Find memory type (helper not included here)
    // allocInfo.memoryTypeIndex = FindMemoryType(...);
    // For now, set to 0 (user must set correctly)
    allocInfo.memoryTypeIndex = 0;
    if (vkAllocateMemory(device, &allocInfo, nullptr, &out.memory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate depth image memory");
    vkBindImageMemory(device, out.image, out.memory, 0);
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = out.image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device, &viewInfo, nullptr, &out.view) != VK_SUCCESS)
        throw std::runtime_error("Failed to create depth image view");
}

void DestroyDepthResources(VkDevice device, DepthResources& res) {
    if (res.view) vkDestroyImageView(device, res.view, nullptr);
    if (res.image) vkDestroyImage(device, res.image, nullptr);
    if (res.memory) vkFreeMemory(device, res.memory, nullptr);
    res.view = VK_NULL_HANDLE;
    res.image = VK_NULL_HANDLE;
    res.memory = VK_NULL_HANDLE;
}

// --- CoreRendering: Double/Triple Buffering ---
void CreateSyncObjects(VkDevice device, int maxFramesInFlight, SyncObjects& sync) {
    sync.imageAvailableSemaphores.resize(maxFramesInFlight);
    sync.renderFinishedSemaphores.resize(maxFramesInFlight);
    sync.inFlightFences.resize(maxFramesInFlight);
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (int i = 0; i < maxFramesInFlight; ++i) {
        if (vkCreateSemaphore(device, &semInfo, nullptr, &sync.imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(device, &semInfo, nullptr, &sync.renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(device, &fenceInfo, nullptr, &sync.inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create sync objects");
        }
    }
}
void DestroySyncObjects(VkDevice device, SyncObjects& sync) {
    for (auto s : sync.imageAvailableSemaphores) if (s) vkDestroySemaphore(device, s, nullptr);
    for (auto s : sync.renderFinishedSemaphores) if (s) vkDestroySemaphore(device, s, nullptr);
    for (auto f : sync.inFlightFences) if (f) vkDestroyFence(device, f, nullptr);
    sync.imageAvailableSemaphores.clear();
    sync.renderFinishedSemaphores.clear();
    sync.inFlightFences.clear();
}

// --- CoreRendering: Descriptor Management ---
void CreateUBODescriptorPool(VkDevice device, uint32_t numSets, VkDescriptorPool& pool) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = numSets;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = numSets;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create UBO descriptor pool");
}
void CreateSamplerDescriptorPool(VkDevice device, uint32_t numSets, VkDescriptorPool& pool) {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = numSets;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = numSets;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create sampler descriptor pool");
}
void DestroyDescriptorPools(VkDevice device, DescriptorPools& pools) {
    if (pools.uboPool) vkDestroyDescriptorPool(device, pools.uboPool, nullptr);
    if (pools.samplerPool) vkDestroyDescriptorPool(device, pools.samplerPool, nullptr);
    pools.uboPool = VK_NULL_HANDLE;
    pools.samplerPool = VK_NULL_HANDLE;
}

// --- CoreRendering: Swapchain Recreation ---
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
) {
    // Destroy old image views
    for (auto view : swapchainRes.imageViews) {
        if (view) vkDestroyImageView(device, view, nullptr);
    }
    swapchainRes.imageViews.clear();
    // Destroy old swapchain
    if (swapchainRes.swapchain) {
        vkDestroySwapchainKHR(device, swapchainRes.swapchain, nullptr);
        swapchainRes.swapchain = VK_NULL_HANDLE;
    }
    // Query surface capabilities
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &caps);
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surfaceFormat = f;
            break;
        }
    }
    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& pm : presentModes) {
        if (pm == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = pm;
            break;
        }
    }
    VkExtent2D ext = windowExtent;
    ext.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, ext.width));
    ext.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, ext.height));
    uint32_t imageCount = caps.minImageCount + 1;
    if (caps.maxImageCount > 0 && imageCount > caps.maxImageCount)
        imageCount = caps.maxImageCount;
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = ext;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = caps.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;
    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchainRes.swapchain) != VK_SUCCESS)
        throw std::runtime_error("Failed to create swapchain");
    vkGetSwapchainImagesKHR(device, swapchainRes.swapchain, &imageCount, nullptr);
    swapchainRes.images.resize(imageCount);
    vkGetSwapchainImagesKHR(device, swapchainRes.swapchain, &imageCount, swapchainRes.images.data());
    swapchainRes.imageFormat = surfaceFormat.format;
    swapchainRes.extent = ext;
    // Create image views
    swapchainRes.imageViews.resize(swapchainRes.images.size());
    for (size_t i = 0; i < swapchainRes.images.size(); ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = swapchainRes.images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = swapchainRes.imageFormat;
        viewInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(device, &viewInfo, nullptr, &swapchainRes.imageViews[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create image view");
    }
    // Recreate depth resources
    depthRes.format = FindSupportedDepthFormat(physicalDevice);
    CreateDepthResources(device, physicalDevice, ext, depthRes.format, depthRes, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
    // Recreate framebuffers
    framebuffers.resize(swapchainRes.imageViews.size());
    for (size_t i = 0; i < swapchainRes.imageViews.size(); ++i) {
        VkImageView attachments[] = { swapchainRes.imageViews[i], depthRes.view };
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = renderPass;
        fbInfo.attachmentCount = 2;
        fbInfo.pAttachments = attachments;
        fbInfo.width = ext.width;
        fbInfo.height = ext.height;
        fbInfo.layers = 1;
        if (vkCreateFramebuffer(device, &fbInfo, nullptr, &framebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create framebuffer");
    }
}
