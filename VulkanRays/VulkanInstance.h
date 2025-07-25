#pragma once
#include <vulkan/vulkan.h>
#include <SDL.h>

class VulkanInstance {
public:
    VulkanInstance(SDL_Window* window, bool enableValidation);
    ~VulkanInstance();
    VkInstance getInstance() const;
    VkSurfaceKHR getSurface() const;
    VkDebugUtilsMessengerEXT getDebugMessenger() const;
private:
    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    void setupDebugMessenger(bool enableValidation);
    // ...other members...
};
