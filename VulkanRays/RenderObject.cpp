#include "RenderObject.h"
#include <cstring>
#include <vector>
#include <cmath>

// --- RenderObject transform implementation ---
Mat4 RenderObject::getModelMatrix() const {
    // Compose scale, then rotation (Z, Y, X), then translation
    Mat4 S = {};
    for (int i = 0; i < 16; ++i) S.m[i] = 0.0f;
    S.m[0] = scale[0]; S.m[5] = scale[1]; S.m[10] = scale[2]; S.m[15] = 1.0f;
    Mat4 Rx = rotationX(rotation[0]);
    Mat4 Ry = rotationY(rotation[1]);
    Mat4 Rz = rotationZ(rotation[2]);
    Mat4 R = mat4_mul(Rz, mat4_mul(Ry, Rx));
    Mat4 SR = mat4_mul(R, S);
    Mat4 M = SR;
    // Apply translation
    M.m[12] = position[0];
    M.m[13] = position[1];
    M.m[14] = position[2];
    return M;
}

// Explicit member definitions for PyramidObject
PyramidObject::PyramidObject()
    : vertexBuffer(nullptr), indexBuffer(nullptr), indexCount(0) {}
PyramidObject::~PyramidObject() = default;

void PyramidObject::createBuffers(VulkanDevice& device, VkPhysicalDevice physicalDevice) {
    float s = 1.0f;
    Vertex vertices[5] = {
        {{-0.5f * s, 0.0f, -0.5f * s}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f * s, 0.0f, -0.5f * s}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f * s, 0.0f,  0.5f * s}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f * s, 0.0f,  0.5f * s}, {1.0f, 1.0f, 0.0f}},
        {{ 0.0f, -1.0f * s,  0.0f}, {1.0f, 1.0f, 1.0f}}
    };
    uint16_t indices[] = {
        0, 1, 4, 1, 2, 4, 2, 3, 4, 3, 0, 4, 0, 2, 1, 0, 3, 2
    };
    indexCount = sizeof(indices) / sizeof(indices[0]);
    VkDeviceSize vsize = sizeof(vertices);
    VkDeviceSize isize = sizeof(indices);
    vertexBuffer = std::make_unique<VulkanBuffer>(
        device, physicalDevice, vsize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    vertexBuffer->uploadData(vertices, vsize);
    indexBuffer = std::make_unique<VulkanBuffer>(
        device, physicalDevice, isize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    indexBuffer->uploadData(indices, isize);
}

void PyramidObject::recordDraw(VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet descriptorSet) {
    VkBuffer vbufs[] = { vertexBuffer->getBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vbufs, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
}

GridObject::GridObject(int gridSize_, float gridSpacing_)
    : gridSize(gridSize_), gridSpacing(gridSpacing_) {}
GridObject::~GridObject() = default;

void GridObject::createBuffers(VulkanDevice& device, VkPhysicalDevice physicalDevice) {
    constexpr float gridY = -0.001f;
    std::vector<Vertex> gridVertices;
    std::vector<uint16_t> gridIndices;
    // Lines parallel to X (varying Z)
    for (int i = -gridSize; i <= gridSize; ++i) {
        float z = i * gridSpacing;
        gridVertices.push_back({ {-gridSize * gridSpacing, gridY, z}, {0.5f, 0.5f, 0.5f} });
        gridVertices.push_back({ { gridSize * gridSpacing, gridY, z}, {0.5f, 0.5f, 0.5f} });
        uint16_t idx = (uint16_t)gridVertices.size();
        gridIndices.push_back(idx - 2);
        gridIndices.push_back(idx - 1);
    }
    // Lines parallel to Z (varying X)
    for (int i = -gridSize; i <= gridSize; ++i) {
        float x = i * gridSpacing;
        gridVertices.push_back({ {x, gridY, -gridSize * gridSpacing}, {0.5f, 0.5f, 0.5f} });
        gridVertices.push_back({ {x, gridY,  gridSize * gridSpacing}, {0.5f, 0.5f, 0.5f} });
        uint16_t idx = (uint16_t)gridVertices.size();
        gridIndices.push_back(idx - 2);
        gridIndices.push_back(idx - 1);
    }
    indexCount = static_cast<uint32_t>(gridIndices.size());
    VkDeviceSize vsize = sizeof(Vertex) * gridVertices.size();
    VkDeviceSize isize = sizeof(uint16_t) * gridIndices.size();
    vertexBuffer = std::make_unique<VulkanBuffer>(
        device, physicalDevice, vsize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    vertexBuffer->uploadData(gridVertices.data(), vsize);
    indexBuffer = std::make_unique<VulkanBuffer>(
        device, physicalDevice, isize,
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
    );
    indexBuffer->uploadData(gridIndices.data(), isize);
}

void GridObject::recordDraw(VkCommandBuffer cmd, VkPipelineLayout layout, VkDescriptorSet descriptorSet) {
    VkBuffer vbufs[] = { vertexBuffer->getBuffer() };
    VkDeviceSize offsets[] = { 0 };
    vkCmdBindVertexBuffers(cmd, 0, 1, vbufs, offsets);
    vkCmdBindIndexBuffer(cmd, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT16);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, 1, &descriptorSet, 0, nullptr);
    vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);
}
