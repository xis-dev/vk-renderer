#pragma once

#include <vector>

#include "volk.h"

class PipelineBuilder {
    VkPipelineCreateFlags m_flags{};
    std::vector<VkPipelineShaderStageCreateInfo> m_shaderStages{};
    VkPipelineVertexInputStateCreateInfo m_vertexInput{};
    VkPipelineInputAssemblyStateCreateInfo m_inputAssembly{};
    VkPipelineViewportStateCreateInfo m_viewportState{};
    VkPipelineRasterizationStateCreateInfo m_rasterization{};
    VkPipelineMultisampleStateCreateInfo m_multisample{};
    VkPipelineDepthStencilStateCreateInfo m_depthStencil{};
    VkPipelineColorBlendStateCreateInfo m_colorBlend{};
    VkPipelineDynamicStateCreateInfo m_dynamicState{};

    VkPipelineLayout m_layout{};

    VkPipeline build_pipeline();
};


