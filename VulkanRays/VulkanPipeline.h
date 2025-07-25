#pragma once
#include <vulkan/vulkan.h>

class VulkanPipeline {
public:
    enum class Topology {
        Triangles,
        Lines
    };
    VulkanPipeline(VkDevice device, VkExtent2D extent, VkRenderPass renderPass, VkDescriptorSetLayout descriptorSetLayout, Topology topology = Topology::Triangles);
    ~VulkanPipeline();
    VkPipeline getGraphicsPipeline() const { return graphicsPipeline; }
    VkPipelineLayout getPipelineLayout() const { return pipelineLayout; }
private:
    VkDevice device;
    VkPipeline graphicsPipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
};
