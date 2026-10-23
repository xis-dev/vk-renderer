#pragma once

#include "volk.h"
#include "glm/vec3.hpp"

struct Vertex {
    glm::vec3 position{};
    glm::vec3 color{};

    static VkVertexInputBindingDescription getBindingDescription() {
        return {.binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX};
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescription() {
        return {
            VkVertexInputAttributeDescription{
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(Vertex, position)
            },

            VkVertexInputAttributeDescription{
                .location = 0,
                .binding = 1,
                .format = VK_FORMAT_R32G32B32_SFLOAT,
                .offset = offsetof(Vertex, color)
            },
        };
    }
};

class Mesh {

};

