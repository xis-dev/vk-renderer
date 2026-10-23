#pragma once

#include "volk.h"
#include "SDL3/SDL.h"
#include "vk_mem_alloc.h"

#include <array>

#include "mesh.h"
#include "utility.h"

inline void transition_image_layout(
    VkCommandBuffer cmdBf,
    VkImage image,
    VkImageLayout oldLayout,
    VkImageLayout newLayout,
    VkAccessFlags2 srcAccessMask,
    VkAccessFlags2 dstAccessMask,
    VkPipelineStageFlags2 srcStageMask,
    VkPipelineStageFlags2 dstStageMask);


constexpr int MAX_FRAMES_IN_FLIGHT = 2;
struct Frame {
    VkCommandBuffer commandBuffer;
    // Signal on presentation finished
    VkSemaphore presentSemaphore;
    VkFence drawFence;
};

class Engine {
    // Vulkan instance objects
    VkInstance m_instance{};
    VkDevice m_device{};
    VkPhysicalDevice m_physicalDevice{};

    std::pair<VkQueue, uint32_t> m_mainQueueWithIndex{};

    // Window & Surface
    SDL_Window* m_window{nullptr};
    VkExtent2D m_windowExtent{};
    VkSurfaceKHR m_surface{};

    // Swapchain
    VkSwapchainKHR m_swapchain{};
    std::vector<VkImage> m_swapchainImages{};
    std::vector<VkImageView> m_swapchainImageViews{};
    std::vector<VkResult> m_swapchainResults{};
    std::vector<VkSemaphore> m_swapchainRenderSemaphores{};
    VkExtent2D m_swapchainExtent{};


    // Pipelines
    VkPipeline m_trianglePipeline{nullptr};
    VkPipelineLayout m_triangleLayout{nullptr};

    // Memory allocator
    VmaAllocator m_allocator{};

    // Commands
    VkCommandPool m_commandPool{nullptr};

    // Buffers
    VkBuffer m_vertexBuffer{};

    std::vector<Frame> m_frames{MAX_FRAMES_IN_FLIGHT};
    uint32_t frameCount{0};
    // Vert buffer array
    std::vector<Vertex> vertices {
        Vertex{
            .position = {-0.5f, 0.5f, 0.0f},
            .color = {1.0f, 0.0f, 0.0f}
        },

        Vertex{
            .position = {0.0f, -0.5f, 0.0f},
            .color = {0.0f, 1.0f, 0.0f}
        },

        Vertex{
            .position = {0.5f, 0.5f, 0.0f},
            .color = {0.0f, 0.0f, 1.0f}
        },
    };

    bool m_bStopRendering{};
public:
    void init(int w, int h);

    void run();

    void cleanup();

private:
    VrUtil::DeletionQueue m_mainDeletionQueue{};

    void init_vulkan();
    void init_device();
    void init_allocator();
    void init_swapchain();
    void init_pipelines();
    void init_commands();
    void init_syncObjects();

    uint32_t getCurrentFrame() const;

    void draw();



#ifdef DEBUG_MODE
    // Debug structures
private:

    const std::array<const char*, 1> m_validationLayers{
        "VK_LAYER_KHRONOS_validation"
    };

    VkDebugUtilsMessengerEXT m_debugMessenger{};
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    void setupDebugMessenger();

#endif

};
