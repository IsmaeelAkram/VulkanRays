#pragma once
#include <SDL.h>
#include "VulkanInstance.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanPipeline.h"
#include "VulkanBuffer.h"
#include <chrono>

class VulkanApp {
public:
    VulkanApp();
    ~VulkanApp();
    int run();
private:
    SDL_Window* window = nullptr;
    VulkanInstance* vkInstance = nullptr;
    VulkanDevice* vkDevice = nullptr;
    VulkanSwapchain* swapchain = nullptr;
    VulkanPipeline* pipeline = nullptr;
    VulkanBuffer* vertexBuffer = nullptr;
    VulkanBuffer* indexBuffer = nullptr;
    VulkanBuffer* mvpBuffer = nullptr;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    std::chrono::steady_clock::time_point startTime;
    float g_pitchAngle = 0.0f;
    float g_yawAngle = 0.0f;
    bool g_dragging = false;
    int g_lastMouseX = 0, g_lastMouseY = 0;
    void mainLoop();
    void handleEvents(bool& running);
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void createDescriptorSet();
    void updateMVPBuffer();
    void createBuffers();
    void createRenderPass();
    void createFramebuffers();
    void createCommandPool();
    void createCommandBuffers();
    void createSyncObjects();
    void recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex);
};
