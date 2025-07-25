#pragma once
#include <vulkan/vulkan.h>
#include <SDL.h>
#include <vector>
#include "VulkanDevice.h"

class VulkanSwapchain {
public:
    VulkanSwapchain(VulkanDevice& device, VkSurfaceKHR surface, SDL_Window* window);
    ~VulkanSwapchain();
    VkSwapchainKHR getSwapchain() const { return swapchain; }
    VkFormat getImageFormat() const { return imageFormat; }
    VkExtent2D getExtent() const { return extent; }
    const std::vector<VkImage>& getImages() const { return images; }
    const std::vector<VkImageView>& getImageViews() const { return imageViews; }
private:
    VulkanDevice& device;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    VkFormat imageFormat;
    VkExtent2D extent;
    void createSwapchain(VkSurfaceKHR surface, SDL_Window* window);
    void createImageViews();
};
