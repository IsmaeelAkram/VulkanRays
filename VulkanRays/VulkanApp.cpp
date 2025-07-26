#include "VulkanApp.h"
#include <iostream>
#include <stdexcept>
#include "MathUtils.h"
#include <vector>
#include "imgui.h"
#include "backends/imgui_impl_sdl2.h"
#include "backends/imgui_impl_vulkan.h"
#include "RenderObject.h"

VulkanApp::VulkanApp() {}
VulkanApp::~VulkanApp() {
    if (vkDevice && vkDevice->getDevice()) {
        vkDeviceWaitIdle(vkDevice->getDevice());
    }
    cleanupVulkanResources();
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
    // Destroy depth resources
    destroyDepthResources();
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
    createDepthResources();
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
    // Modular: create all render objects
    renderObjects.clear();
    renderObjects.push_back(std::make_unique<GridObject>(20, 0.5f));
    // Create 3 pyramids side by side
    auto pyramid1 = std::make_unique<PyramidObject>();
    pyramid1->setPosition(-1.5f, 0.0f, 0.0f);
    auto pyramid2 = std::make_unique<PyramidObject>();
    pyramid2->setPosition(0.0f, 0.0f, 0.0f);
    auto pyramid3 = std::make_unique<PyramidObject>();
    pyramid3->setPosition(1.5f, 0.0f, 0.0f);
    renderObjects.push_back(std::move(pyramid1));
    renderObjects.push_back(std::move(pyramid2));
    renderObjects.push_back(std::move(pyramid3));
    for (auto& obj : renderObjects) {
        obj->createBuffers(*vkDevice, vkDevice->getPhysicalDevice());
    }
}

void VulkanApp::createDescriptorSet() {
    for (auto& obj : renderObjects) {
        obj->mvpBuffer = new VulkanBuffer(
            *vkDevice,
            vkDevice->getPhysicalDevice(),
            sizeof(Mat4),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &descriptorSetLayout;
        if (vkAllocateDescriptorSets(vkDevice->getDevice(), &allocInfo, &obj->descriptorSet) != VK_SUCCESS)
            throw std::runtime_error("Failed to allocate descriptor set");
        VkDescriptorBufferInfo bufferInfo{};
        bufferInfo.buffer = obj->mvpBuffer->getBuffer();
        bufferInfo.offset = 0;
        bufferInfo.range = sizeof(Mat4);
        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = obj->descriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pBufferInfo = &bufferInfo;
        vkUpdateDescriptorSets(vkDevice->getDevice(), 1, &descriptorWrite, 0, nullptr);
    }
}

void VulkanApp::mainLoop() {
    bool running = true;
    size_t imageCount = swapchain->getImages().size();
    uint32_t currentFrame = 0;
    auto lastTime = std::chrono::high_resolution_clock::now();
    while (running) {
        handleEvents(running);
        // Camera movement (was in updateMVPBuffer, now here)
        float moveSpeed = 0.05f;
        float forward[3] = { sinf(camYaw) * cosf(camPitch), sinf(camPitch), -cosf(camYaw) * cosf(camPitch) };
        float right[3] = { cosf(camYaw), 0, sinf(camYaw) };
        if (keyW) { camX += forward[0] * moveSpeed; camY += forward[1] * moveSpeed; camZ += forward[2] * moveSpeed; }
        if (keyS) { camX -= forward[0] * moveSpeed; camY -= forward[1] * moveSpeed; camZ -= forward[2] * moveSpeed; }
        if (keyA) { camX -= right[0] * moveSpeed; camZ -= right[2] * moveSpeed; }
        if (keyD) { camX += right[0] * moveSpeed; camZ += right[2] * moveSpeed; }
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
        ImGui::Render();
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
        // Modular: draw all render objects
        for (auto& obj : renderObjects) {
            VulkanPipeline* usedPipeline = (obj->getTopology() == VulkanPipeline::Topology::Lines) ? gridPipeline : pipeline;
            if (usedPipeline) {
                vkCmdBindPipeline(commandBuffers[imageIndex], VK_PIPELINE_BIND_POINT_GRAPHICS, usedPipeline->getGraphicsPipeline());
                obj->recordDraw(commandBuffers[imageIndex], usedPipeline->getPipelineLayout(), obj->descriptorSet);
            }
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
    size_t numObjects = renderObjects.size();
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = static_cast<uint32_t>(numObjects);
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = static_cast<uint32_t>(numObjects);
    if (vkCreateDescriptorPool(vkDevice->getDevice(), &poolInfo, nullptr, &descriptorPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create descriptor pool");
}
void VulkanApp::updateMVPBuffer() {
    // No-op: all per-object MVP buffer updates are handled in recordCommandBuffer()
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

    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = findDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkAttachmentDescription attachments[2] = { colorAttachment, depthAttachment };
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 2;
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    if (vkCreateRenderPass(vkDevice->getDevice(), &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create render pass");
}
void VulkanApp::createFramebuffers() {
    auto& views = swapchain->getImageViews();
    framebuffers.resize(views.size());
    for (size_t i = 0; i < views.size(); ++i) {
        VkImageView attachments[] = { views[i], depthImageView };
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = renderPass;
        fbInfo.attachmentCount = 2;
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
    VkClearValue clearValues[2];
    clearValues[0].color = { {0.1f, 0.1f, 0.1f, 1.0f} };
    clearValues[1].depthStencil = { 1.0f, 0 };
    VkRenderPassBeginInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpInfo.renderPass = renderPass;
    rpInfo.framebuffer = framebuffers[imageIndex];
    rpInfo.renderArea.offset = {0, 0};
    rpInfo.renderArea.extent = swapchain->getExtent();
    rpInfo.clearValueCount = 2;
    rpInfo.pClearValues = clearValues;
    vkCmdBeginRenderPass(cmd, &rpInfo, VK_SUBPASS_CONTENTS_INLINE);
    // Modular: draw all render objects
    int w = (int)swapchain->getExtent().width, h = (int)swapchain->getExtent().height;
    float aspect = w / (float)h;
    Mat4 proj = perspective(1.0f, aspect, 0.1f, 100.0f);
    float eyeX = camX, eyeY = camY, eyeZ = camZ;
    float forward[3] = { sinf(camYaw) * cosf(camPitch), sinf(camPitch), -cosf(camYaw) * cosf(camPitch) };
    float centerX = camX + forward[0], centerY = camY + forward[1], centerZ = camZ + forward[2];
    float upX = 0, upY = 1, upZ = 0;
    Mat4 view = lookAt(eyeX, eyeY, eyeZ, centerX, centerY, centerZ, upX, upY, upZ);
    for (auto& obj : renderObjects) {
        VulkanPipeline* usedPipeline = (obj->getTopology() == VulkanPipeline::Topology::Lines) ? gridPipeline : pipeline;
        if (usedPipeline) {
            // Per-object MVP
            Mat4 model = obj->getModelMatrix();
            Mat4 mvp = mat4_mul(proj, mat4_mul(view, model));
            obj->mvpBuffer->uploadData(&mvp, sizeof(Mat4));
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, usedPipeline->getGraphicsPipeline());
            obj->recordDraw(cmd, usedPipeline->getPipelineLayout(), obj->descriptorSet);
        }
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

VkFormat VulkanApp::findDepthFormat() {
    const VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };
    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(vkDevice->getPhysicalDevice(), format, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            return format;
    }
    throw std::runtime_error("Failed to find supported depth format");
}

void VulkanApp::createDepthResources() {
    depthFormat = findDepthFormat();
    VkExtent2D extent = swapchain->getExtent();
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = extent.width;
    imageInfo.extent.height = extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateImage(vkDevice->getDevice(), &imageInfo, nullptr, &depthImage) != VK_SUCCESS)
        throw std::runtime_error("Failed to create depth image");
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(vkDevice->getDevice(), depthImage, &memRequirements);
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = vkDevice->findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (vkAllocateMemory(vkDevice->getDevice(), &allocInfo, nullptr, &depthImageMemory) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate depth image memory");
    vkBindImageMemory(vkDevice->getDevice(), depthImage, depthImageMemory, 0);
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(vkDevice->getDevice(), &viewInfo, nullptr, &depthImageView) != VK_SUCCESS)
        throw std::runtime_error("Failed to create depth image view");
}

void VulkanApp::destroyDepthResources() {
    if (depthImageView) {
        vkDestroyImageView(vkDevice->getDevice(), depthImageView, nullptr);
        depthImageView = VK_NULL_HANDLE;
    }
    if (depthImage) {
        vkDestroyImage(vkDevice->getDevice(), depthImage, nullptr);
        depthImage = VK_NULL_HANDLE;
    }
    if (depthImageMemory) {
        vkFreeMemory(vkDevice->getDevice(), depthImageMemory, nullptr);
        depthImageMemory = VK_NULL_HANDLE;
    }
}
