#pragma once
#include <vulkan/vulkan.h>
#include <memory>
#include "VulkanBuffer.h"
#include "VulkanPipeline.h"
#include "MathUtils.h"
#include <vector>

// Vertex struct reused for all objects
struct Vertex {
    float pos[3];
    float color[3];
};

// Abstract base class for all renderable objects
class RenderObject {
public:
    virtual ~RenderObject() {
        if (mvpBuffer) {
            mvpBuffer->destroy();
            delete mvpBuffer;
            mvpBuffer = nullptr;
        }
    }
    virtual void createBuffers(VulkanDevice& device, VkPhysicalDevice physicalDevice) = 0;
    virtual void recordDraw(VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet descriptorSet) = 0;
    // Optionally override to specify pipeline topology
    virtual VulkanPipeline::Topology getTopology() const { return VulkanPipeline::Topology::Triangles; }

    // Transform interface
    void setPosition(float x, float y, float z) { position[0]=x; position[1]=y; position[2]=z; }
    void setRotation(float pitch, float yaw, float roll) { rotation[0]=pitch; rotation[1]=yaw; rotation[2]=roll; }
    void setScale(float sx, float sy, float sz) { scale[0]=sx; scale[1]=sy; scale[2]=sz; }
    const float* getPosition() const { return position; }
    const float* getRotation() const { return rotation; }
    const float* getScale() const { return scale; }
    // Model matrix from transform
    virtual Mat4 getModelMatrix() const;
    // Per-object MVP buffer and descriptor set
    VulkanBuffer* mvpBuffer = nullptr;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
protected:
    float position[3] = {0,0,0};
    float rotation[3] = {0,0,0}; // pitch, yaw, roll (radians)
    float scale[3] = {1,1,1};
};

// Pyramid renderable object
class PyramidObject : public RenderObject {
public:
    PyramidObject();
    ~PyramidObject() override;
    void createBuffers(VulkanDevice& device, VkPhysicalDevice physicalDevice) override;
    void recordDraw(VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet descriptorSet) override;
private:
    std::unique_ptr<VulkanBuffer> vertexBuffer = nullptr;
    std::unique_ptr<VulkanBuffer> indexBuffer = nullptr;
    uint32_t indexCount = 0;
};

// Grid renderable object
class GridObject : public RenderObject {
public:
    GridObject(int gridSize = 20, float gridSpacing = 0.5f);
    ~GridObject() override;
    void createBuffers(VulkanDevice& device, VkPhysicalDevice physicalDevice) override;
    void recordDraw(VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet descriptorSet) override;
    VulkanPipeline::Topology getTopology() const override { return VulkanPipeline::Topology::Lines; }
private:
    int gridSize;
    float gridSpacing;
    std::unique_ptr<VulkanBuffer> vertexBuffer;
    std::unique_ptr<VulkanBuffer> indexBuffer;
    uint32_t indexCount = 0;
};
