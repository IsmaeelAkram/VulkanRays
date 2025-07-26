#pragma once
#include <SDL.h>
#include "VulkanInstance.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanPipeline.h"
#include "VulkanBuffer.h"
#include <chrono>
#include <vector>
#include <memory>
#include "RenderObject.h"
// ImGui forward declarations
struct ImGui_ImplVulkan_InitInfo;

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
    VulkanPipeline* pipeline = nullptr; // For triangles (pyramid)
    VulkanPipeline* gridPipeline = nullptr; // For lines (grid)
    VulkanBuffer* mvpBuffer = nullptr;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    std::chrono::steady_clock::time_point startTime;
    float g_pitchAngle = 0.0f;
    float g_yawAngle = 0.0f;
    bool g_dragging = false;
    int g_lastMouseX = 0, g_lastMouseY = 0;

    // Camera state
    float camX = 0.0f, camY = 1.0f, camZ = 2.5f; // Camera position
    float camYaw = 0.0f, camPitch = 0.0f;        // Camera orientation (radians)
    bool keyW = false, keyA = false, keyS = false, keyD = false; // WASD state
    bool mouseCaptured = false;
    int lastMouseX = 0, lastMouseY = 0;

    // --- Vulkan resources ---
    VkRenderPass renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> framebuffers;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;

    // --- Depth resources ---
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthImageMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;

    VkFormat findDepthFormat();
    void createDepthResources();
    void destroyDepthResources();

    // --- ImGui integration ---
    VkDescriptorPool imguiPool = VK_NULL_HANDLE;
    void initImGui();
    void shutdownImGui();
    void recordImGui(VkCommandBuffer cmd);

    // --- FPS timing ---
    double lastFrameTime = 0.0;
    double fps = 0.0;
    double frameAccumulator = 0.0;
    int frameCount = 0;

    // Modular render objects
    std::vector<std::unique_ptr<RenderObject>> renderObjects;

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
    void cleanupVulkanResources();
};
