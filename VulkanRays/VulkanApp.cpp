#include "VulkanApp.h"
#include <iostream>
#include <stdexcept>
#include "MathUtils.h"
#include <vector>
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"

struct Vertex {
    float pos[3];
    float color[3];
};

VulkanApp::VulkanApp() {}
VulkanApp::~VulkanApp() {
    if (vkDevice && vkDevice->getDevice()) {
        vkDeviceWaitIdle(vkDevice->getDevice());
    }
    cleanupVulkanResources();
    if (vertexBuffer) { vertexBuffer->destroy(); delete vertexBuffer; vertexBuffer = nullptr; }
    if (indexBuffer) { indexBuffer->destroy(); delete indexBuffer; indexBuffer = nullptr; }
    if (mvpBuffer) { mvpBuffer->destroy(); delete mvpBuffer; mvpBuffer = nullptr; }
    if (gridVertexBuffer) { gridVertexBuffer->destroy(); delete gridVertexBuffer; gridVertexBuffer = nullptr; }
    if (gridIndexBuffer) { gridIndexBuffer->destroy(); delete gridIndexBuffer; gridIndexBuffer = nullptr; }
    if (pipeline) delete pipeline;
    if (gridPipeline) delete gridPipeline;
    if (swapchain) delete swapchain;
    if (vkDevice) delete vkDevice;
    if (vkInstance) delete vkInstance;
    if (window) SDL_DestroyWindow(window);
    SDL_Quit();
}

void VulkanApp::cleanupVulkanResources() {
    // Destroy framebuffers
    for (auto fb : framebuffers) {
        if (fb) vkDestroyFramebuffer(vkDevice->getDevice(), fb, nullptr);
    }
    framebuffers.clear();
    // Destroy command buffers
    if (!commandBuffers.empty() && commandPool) {
        vkFreeCommandBuffers(vkDevice->getDevice(), commandPool, (uint32_t)commandBuffers.size(), commandBuffers.data());
    }
    commandBuffers.clear();
    // Destroy command pool
    if (commandPool) {
        vkDestroyCommandPool(vkDevice->getDevice(), commandPool, nullptr);
        commandPool = VK_NULL_HANDLE;
    }
    // Destroy sync objects
    for (auto s : imageAvailableSemaphores) {
        if (s) vkDestroySemaphore(vkDevice->getDevice(), s, nullptr);
    }
    imageAvailableSemaphores.clear();
    for (auto s : renderFinishedSemaphores) {
        if (s) vkDestroySemaphore(vkDevice->getDevice(), s, nullptr);
    }
    renderFinishedSemaphores.clear();
    for (auto f : inFlightFences) {
        if (f) vkDestroyFence(vkDevice->getDevice(), f, nullptr);
    }
    inFlightFences.clear();
    // Destroy descriptor pool
    if (descriptorPool) {
        vkDestroyDescriptorPool(vkDevice->getDevice(), descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
    // Destroy descriptor set layout
    if (descriptorSetLayout) {
        vkDestroyDescriptorSetLayout(vkDevice->getDevice(), descriptorSetLayout, nullptr);
        descriptorSetLayout = VK_NULL_HANDLE;
    }
    // Destroy render pass
    if (renderPass) {
        vkDestroyRenderPass(vkDevice->getDevice(), renderPass, nullptr);
        renderPass = VK_NULL_HANDLE;
    }
}

// --- VulkanApp private members for rendering ---
VkRenderPass renderPass = VK_NULL_HANDLE;
std::vector<VkFramebuffer> framebuffers;
VkCommandPool commandPool = VK_NULL_HANDLE;
std::vector<VkCommandBuffer> commandBuffers;
VkSemaphore imageAvailableSemaphore = VK_NULL_HANDLE;
VkSemaphore renderFinishedSemaphore = VK_NULL_HANDLE;
VkFence inFlightFence = VK_NULL_HANDLE;

int VulkanApp::run() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL Init Error: " << SDL_GetError() << "\n";
        return 1;
    }
    window = SDL_CreateWindow("VulkanRays", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280,720, SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window) {
        std::cerr << "SDL_CreateWindow failed\n";
        return 1;
    }
    vkInstance = new VulkanInstance(window, true);
    vkDevice = new VulkanDevice(vkInstance->getInstance(), vkInstance->getSurface());
    swapchain = new VulkanSwapchain(*vkDevice, vkInstance->getSurface(), window);
    createDescriptorSetLayout();
    createBuffers();
    createDescriptorPool();
    createDescriptorSet();
    createRenderPass();
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
    // Create pipelines: one for triangles (pyramid), one for lines (grid)
    pipeline = new VulkanPipeline(vkDevice->getDevice(), swapchain->getExtent(), renderPass, descriptorSetLayout, VulkanPipeline::Topology::Triangles);
    gridPipeline = new VulkanPipeline(vkDevice->getDevice(), swapchain->getExtent(), renderPass, descriptorSetLayout, VulkanPipeline::Topology::Lines);
    initImGui();
    mainLoop();
    shutdownImGui();
    return 0;
}

void VulkanApp::createBuffers() {
    // Pyramid vertices (base at y=0, tip at y=1)
    float s = pyramidScale;
    Vertex vertices[5] = {
        {{-0.5f * s, 0.0f, -0.5f * s}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f * s, 0.0f, -0.5f * s}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f * s, 0.0f,  0.5f * s}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f * s, 0.0f,  0.5f * s}, {1.0f, 1.0f, 0.0f}},
        {{ 0.0f, -1.0f * s,  0.0f}, {1.0f, 1.0f, 1.0f}} // Tip moved to y = -1.0f * s
    };
    uint16_t indices[] = {
        0, 1, 4, 1, 2, 4, 2, 3, 4, 3, 0, 4, 0, 2, 1, 0, 3, 2
    };
    VkDeviceSize vsize = sizeof(vertices);
    VkDeviceSize isize = sizeof(indices);
    vertexBuffer = new VulkanBuffer(
        *vkDevice,
        vkDevice->getPhysicalDevice(),
        vsize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    vertexBuffer->uploadData(vertices, vsize);
    indexBuffer = new VulkanBuffer(
        *vkDevice,
        vkDevice->getPhysicalDevice(),
        isize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    indexBuffer->uploadData(indices, isize);
    mvpBuffer = new VulkanBuffer(
        *vkDevice,
        vkDevice->getPhysicalDevice(),
        sizeof(Mat4),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    createGridBuffers();
}

void VulkanApp::createGridBuffers() {
    // Create a grid in the XZ plane, centered at origin, y=-0.001 to avoid z-fighting
    constexpr int gridSize = 20;
    constexpr float gridSpacing = 0.5f;
    constexpr int numLines = gridSize * 2 + 1;
    constexpr float gridY = -0.001f; // Offset grid slightly below y=0
    std::vector<Vertex> gridVertices;
    std::vector<uint16_t> gridIndices;
    // Lines parallel to X (varying Z)
    for (int i = -gridSize; i <= gridSize; ++i) {
        float z = i * gridSpacing;
        gridVertices.push_back({{-gridSize * gridSpacing, gridY, z}, {0.5f, 0.5f, 0.5f}});
        gridVertices.push_back({{ gridSize * gridSpacing, gridY, z}, {0.5f, 0.5f, 0.5f}});
        uint16_t idx = (uint16_t)gridVertices.size();
        gridIndices.push_back(idx - 2);
        gridIndices.push_back(idx - 1);
    }
    // Lines parallel to Z (varying X)
    for (int i = -gridSize; i <= gridSize; ++i) {
        float x = i * gridSpacing;
        gridVertices.push_back({{x, gridY, -gridSize * gridSpacing}, {0.5f, 0.5f, 0.5f}});
        gridVertices.push_back({{x, gridY,  gridSize * gridSpacing}, {0.5f, 0.5f, 0.5f}});
        uint16_t idx = (uint16_t)gridVertices.size();
        gridIndices.push_back(idx - 2);
        gridIndices.push_back(idx - 1);
    }
    VkDeviceSize vsize = sizeof(Vertex) * gridVertices.size();
    VkDeviceSize isize = sizeof(uint16_t) * gridIndices.size();
    gridVertexBuffer = new VulkanBuffer(
        *vkDevice,
        vkDevice->getPhysicalDevice(),
        vsize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    gridVertexBuffer->uploadData(gridVertices.data(), vsize);
    gridIndexBuffer = new VulkanBuffer(
        *vkDevice,
        vkDevice->getPhysicalDevice(),
        isize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    gridIndexBuffer->uploadData(gridIndices.data(), isize);
}

void VulkanApp::mainLoop() {
    bool running = true;
    size_t imageCount = swapchain->getImages().size();
    uint32_t currentFrame = 0;
    auto lastTime = std::chrono::high_resolution_clock::now();
    float lastPyramidScale = pyramidScale;
    while (running) {
        handleEvents(running);
        updateMVPBuffer();
        // FPS calculation
        auto now = std::chrono::high_resolution_clock::now();
        double delta = std::chrono::duration<double>(now - lastTime).count();
        lastTime = now;
        frameAccumulator += delta;
        frameCount++;
        if (frameAccumulator >= 1.0) {
            fps = frameCount / frameAccumulator;
            frameAccumulator = 0.0;
            frameCount = 0;
        }
        vkWaitForFences(vkDevice->getDevice(), 1, &inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
        vkResetFences(vkDevice->getDevice(), 1, &inFlightFences[currentFrame]);
        uint32_t imageIndex;
        vkAcquireNextImageKHR(vkDevice->getDevice(), swapchain->getSwapchain(), UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
        vkResetCommandBuffer(commandBuffers[imageIndex], 0);
        // Start ImGui frame
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame(); // No arguments for latest ImGui
        ImGui::NewFrame();
        // FPS overlay window
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.35f);
        ImGui::Begin("FPS", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
        ImGui::Text("FPS: %.1f", fps);
        ImGui::End();
        // Pyramid scale slider
        ImGui::SetNextWindowPos(ImVec2(10, 60), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.35f);
        ImGui::Begin("Pyramid Controls", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);
        ImGui::SliderFloat("Cone scale", &pyramidScale, 0.1f, 3.0f, "%.2f");
        ImGui::End();
        ImGui::Render();
        // If scale changed, recreate pyramid vertex buffer
        if (pyramidScale != lastPyramidScale) {
            if (vertexBuffer) { vertexBuffer->destroy(); delete vertexBuffer; vertexBuffer = nullptr; }
            createBuffers();
            lastPyramidScale = pyramidScale;
        }
        recordCommandBuffer(commandBuffers[imageIndex], imageIndex);
        // Record ImGui draw data
        vkResetCommandBuffer(commandBuffers[imageIndex], 0);
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(commandBuffers[imageIndex], &beginInfo);
        VkClearValue clearColor = { {0.1f, 0.1f, 0.1f, 1.0f} };
        VkRenderPassBeginInfo rpInfo{};
        rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        rpInfo.renderPass = renderPass;
        rpInfo.framebuffer = framebuffers[imageIndex];
        rpInfo.renderArea.offset = {0, 0};
        rpInfo.renderArea.extent = swapchain->getExtent();
        rpInfo.clearValueCount = 1;
        rpInfo.pClearValues = &clearColor;
        vkCmdBeginRenderPass(commandBuffers[imageIndex], &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
        // Draw grid and pyramid
        if (gridVertexBuffer && gridIndexBuffer && gridPipeline) {
            VkBuffer gridVbufs[] = { gridVertexBuffer->getBuffer() };
            VkDeviceSize gridOffsets[] = { 0 };
            vkCmdBindPipeline(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, gridPipeline->getGraphicsPipeline());
            vkCmdBindVertexBuffers(commandBuffers[imageIndex], 0, 1, gridVbufs, gridOffsets);
            vkCmdBindIndexBuffer(commandBuffers[imageIndex], gridIndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT16);
            vkCmdBindDescriptorSets(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, gridPipeline->getPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);
            vkCmdDrawIndexed(commandBuffers[imageIndex], 2 * (20 * 2 + 1) * 2, 1, 0, 0, 0);
        }
        if (pipeline) {
            VkBuffer vbufs[] = { vertexBuffer->getBuffer() };
            VkDeviceSize offsets[] = { 0 };
            vkCmdBindPipeline(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getGraphicsPipeline());
            vkCmdBindVertexBuffers(commandBuffers[imageIndex], 0, 1, vbufs, offsets);
            vkCmdBindIndexBuffer(commandBuffers[imageIndex], indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT16);
            vkCmdBindDescriptorSets(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);
            vkCmdDrawIndexed(commandBuffers[imageIndex], 18, 1, 0, 0, 0);
        }
        // ImGui draw
        recordImGui(commandBuffers[imageIndex]);
        vkCmdEndRenderPass(commandBuffers[imageIndex]);
        vkEndCommandBuffer(commandBuffers[imageIndex]);
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = { imageAvailableSemaphores[currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[imageIndex];
        VkSemaphore signalSemaphores[] = { renderFinishedSemaphores[currentFrame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        if (vkQueueSubmit(vkDevice->getGraphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) != VK_SUCCESS)
            throw std::runtime_error("Failed to submit draw command buffer");
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        VkSwapchainKHR swapchains[] = { swapchain->getSwapchain() };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;
        vkQueuePresentKHR(vkDevice->getPresentQueue(), &presentInfo);
        currentFrame = (currentFrame + 1) % imageCount;
    }
}

void VulkanApp::handleEvents(bool& running) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL2_ProcessEvent(&event); // Pass events to ImGui
        if (event.type == SDL_QUIT) running = false;
        else if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            bool down = (event.type == SDL_KEYDOWN);
            switch (event.key.keysym.scancode) {
                case SDL_SCANCODE_W: keyW = down; break;
                case SDL_SCANCODE_A: keyA = down; break;
                case SDL_SCANCODE_S: keyS = down; break;
                case SDL_SCANCODE_D: keyD = down; break;
                case SDL_SCANCODE_ESCAPE:
                    if (down && mouseCaptured) {
                        SDL_SetRelativeMouseMode(SDL_FALSE);
                        mouseCaptured = false;
                    }
                    break;
                default: break;
            }
        } else if (event.type == SDL_MOUSEBUTTONDOWN) {
            if (event.button.button == SDL_BUTTON_LEFT && !mouseCaptured) {
                if (!ImGui::GetIO().WantCaptureMouse) {
                    SDL_SetRelativeMouseMode(SDL_TRUE);
                    mouseCaptured = true;
                }
            }
        } else if (event.type == SDL_MOUSEMOTION && mouseCaptured) {
            if (!ImGui::GetIO().WantCaptureMouse) {
                float sensitivity = 0.002f;
                camYaw   += event.motion.xrel * sensitivity;
                camPitch += event.motion.yrel * sensitivity;
                // Clamp pitch
                if (camPitch > 1.5f) camPitch = 1.5f;
                if (camPitch < -1.5f) camPitch = -1.5f;
            }
        }
    }
}

void VulkanApp::createDescriptorSetLayout() {
    VkDescriptorSetLayoutBinding uboLayoutBinding{};
    uboLayoutBinding.binding = 0;
    uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboLayoutBinding.descriptorCount = 1;
    uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    uboLayoutBinding.pImmutableSamplers = nullptr;
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &uboLayoutBinding;
    if (vkCreateDescriptorSetLayout(vkDevice->getDevice(), &layoutInfo, nullptr, &descriptorSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor set layout");
}
void VulkanApp::createDescriptorPool() {
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;
    if (vkCreateDescriptorPool(vkDevice->getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor pool");
}
void VulkanApp::createDescriptorSet() {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &descriptorSetLayout;
    if (vkAllocateDescriptorSets(vkDevice->getDevice(), &allocInfo, &descriptorSet) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate descriptor set");
    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = mvpBuffer->getBuffer();
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(Mat4);
    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(vkDevice->getDevice(), 1, &descriptorWrite, 0, nullptr);
}
void VulkanApp::updateMVPBuffer() {
    int w = (int)swapchain->getExtent().width, h = (int)swapchain->getExtent().height;
    float aspect = w / (float)h;
    Mat4 proj = perspective(1.0f, aspect, 0.1f, 100.0f);

    // Camera movement
    float moveSpeed = 0.05f;
    float forward[3] = { sinf(camYaw) * cosf(camPitch), sinf(camPitch), -cosf(camYaw) * cosf(camPitch) };
    float right[3] = { cosf(camYaw), 0, sinf(camYaw) };
    if (keyW) { camX += forward[0] * moveSpeed; camY += forward[1] * moveSpeed; camZ += forward[2] * moveSpeed; }
    if (keyS) { camX -= forward[0] * moveSpeed; camY -= forward[1] * moveSpeed; camZ -= forward[2] * moveSpeed; }
    if (keyA) { camX -= right[0] * moveSpeed; camZ -= right[2] * moveSpeed; }
    if (keyD) { camX += right[0] * moveSpeed; camZ += right[2] * moveSpeed; }

    float eyeX = camX, eyeY = camY, eyeZ = camZ;
    float centerX = camX + forward[0], centerY = camY + forward[1], centerZ = camZ + forward[2];
    float upX = 0, upY = 1, upZ = 0;
    Mat4 view = lookAt(eyeX, eyeY, eyeZ, centerX, centerY, centerZ, upX, upY, upZ);

    Mat4 model = {};
    for (int i = 0; i < 16; ++i) model.m[i] = (i % 5 == 0) ? 1.0f : 0.0f; // Identity
    Mat4 mvp = mat4_mul(proj, mat4_mul(view, model));
    mvpBuffer->uploadData(&mvp, sizeof(Mat4));
}

void VulkanApp::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchain->getImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    if (vkCreateRenderPass(vkDevice->getDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create render pass");
}
void VulkanApp::createFramebuffers() {
    auto& views = swapchain->getImageViews();
    framebuffers.resize(views.size());
    for (size_t i = 0; i < views.size(); ++i) {
        VkImageView attachments[] = { views[i] };
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = attachments;
        fbInfo.width = swapchain->getExtent().width;
        fbInfo.height = swapchain->getExtent().height;
        fbInfo.layers = 1;
        if (vkCreateFramebuffer(vkDevice->getDevice(), &fbInfo, nullptr, &framebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("Failed to create framebuffer");
    }
}
void VulkanApp::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = vkDevice->getGraphicsQueueFamily();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(vkDevice->getDevice(), &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create command pool");
}
void VulkanApp::createCommandBuffers() {
    commandBuffers.resize(framebuffers.size());
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();
    if (vkAllocateCommandBuffers(vkDevice->getDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate command buffers");
}
void VulkanApp::createSyncObjects() {
    size_t imageCount = swapchain->getImages().size();
    imageAvailableSemaphores.resize(imageCount);
    renderFinishedSemaphores.resize(imageCount);
    inFlightFences.resize(imageCount);
    VkSemaphoreCreateInfo semInfo{};
    semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (size_t i = 0; i < imageCount; ++i) {
        if (vkCreateSemaphore(vkDevice->getDevice(), &semInfo, nullptr, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(vkDevice->getDevice(), &semInfo, nullptr, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(vkDevice->getDevice(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create sync objects");
        }
    }
}
void VulkanApp::recordCommandBuffer(VkCommandBuffer cmd, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmd, &beginInfo);
    VkClearValue clearColor = { {0.1f, 0.1f, 0.1f, 1.0f} };
    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = renderPass;
    rpInfo.framebuffer = framebuffers[imageIndex];
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = swapchain->getExtent();
    rpInfo.clearValueCount = 1;
    rpInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
    // Draw grid (lines)
    if (gridVertexBuffer && gridIndexBuffer && gridPipeline) {
        VkBuffer gridVbufs[] = { gridVertexBuffer->getBuffer() };
        VkDeviceSize gridOffsets[] = { 0 };
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gridPipeline->getGraphicsPipeline());
        vkCmdBindVertexBuffers(cmd, 0, 1, gridVbufs, gridOffsets);
        vkCmdBindIndexBuffer(cmd, gridIndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, gridPipeline->getPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);
        // Grid has 2*(2*gridSize+1)*2 indices
        vkCmdDrawIndexed(cmd, 2 * (20 * 2 + 1) * 2, 1, 0, 0, 0);
    }
    // Draw pyramid
    if (pipeline) {
        VkBuffer vbufs[] = { vertexBuffer->getBuffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getGraphicsPipeline());
        vkCmdBindVertexBuffers(cmd, 0, 1, vbufs, offsets);
        vkCmdBindIndexBuffer(cmd, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT16);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);
        vkCmdDrawIndexed(cmd, 18, 1, 0, 0, 0);
    }
    vkCmdEndRenderPass(cmd);
    vkEndCommandBuffer(cmd);
}

void VulkanApp::initImGui() {
    // Create ImGui descriptor pool (large enough for ImGui needs)
    VkDescriptorPoolSize pool_sizes[] = {
        { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
        { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
        { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };
    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000 * (uint32_t)(sizeof(pool_sizes)/sizeof(pool_sizes[0]));
    pool_info.poolSizeCount = (uint32_t)(sizeof(pool_sizes)/sizeof(pool_sizes[0]));
    pool_info.pPoolSizes = pool_sizes;
    vkCreateDescriptorPool(vkDevice->getDevice(), &pool_info, nullptr, &imguiPool);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGui_ImplSDL2_InitForVulkan(window);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = vkInstance->getInstance();
    init_info.PhysicalDevice = vkDevice->getPhysicalDevice();
    init_info.Device = vkDevice->getDevice();
    init_info.QueueFamily = vkDevice->getGraphicsQueueFamily();
    init_info.Queue = vkDevice->getGraphicsQueue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = imguiPool;
    init_info.Allocator = nullptr;
    init_info.MinImageCount = 2;
    init_info.ImageCount = (uint32_t)swapchain->getImages().size();
    init_info.CheckVkResultFn = nullptr;
    init_info.RenderPass = renderPass;
    ImGui_ImplVulkan_Init(&init_info);
    // NOTE: ImGui_ImplVulkan_CreateFontsTexture and ImGui_ImplVulkan_DestroyFontUploadObjects are not used or needed in this project. If you see errors referencing them, they are stale or from a previous build. No such calls exist in this file.
}

void VulkanApp::shutdownImGui() {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();
    if (imguiPool) {
        vkDestroyDescriptorPool(vkDevice->getDevice(), imguiPool, nullptr);
        imguiPool = VK_NULL_HANDLE;
    }
}

void VulkanApp::recordImGui(VkCommandBuffer cmd) {
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
}
