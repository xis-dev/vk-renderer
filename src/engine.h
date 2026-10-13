#pragma once


#include "SDL3/SDL.h"
#include "vulkan/vulkan_core.h"

#include <array>
#include <functional>
#include <deque>
#include <ranges>

#include "utility.h"

class Engine {
    VkInstance m_instance{};
    VkDevice m_device{};
    VkPhysicalDevice m_physicalDevice{};

    SDL_Window* m_window{nullptr};
    VkExtent2D m_windowExtent{};

    bool m_bStopRendering{};
public:
    void init(int w, int h);

    void run();

    void cleanup();

private:
    VrUtil::DeletionQueue m_mainDeletionQueue{};

    void init_vulkan();

    void draw();

    // Debug structures
#ifdef DEBUG_MODE
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

