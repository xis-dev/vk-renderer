#include "engine.h"

#include "macros.h"

#include "vulkan/vulkan.h"
#include "SDL3/SDL_vulkan.h"

#include <vector>
#include <iostream>

#include "utility.h"

void Engine::init(int w, int h)
{
    SDL_Init(SDL_INIT_VIDEO);
    m_window = SDL_CreateWindow("Vulkan Renderer", w, h, SDL_WINDOW_VULKAN);
    if (!m_window)
    {
        std::cout << "Engine::SDL: Failed to initialize window.\n";
        SDL_Quit();
        return;
    }
    m_windowExtent.width = w;
    m_windowExtent.height = h;

    init_vulkan();
}

void Engine::run()
{
    bool running = true;
    SDL_Event e;

    while (running)
    {
        // SDL Event handling
        while (SDL_PollEvent(&e))
        {
            // Switch on basic types
            switch (e.type)
            {
                case SDL_EVENT_QUIT:
                    running = false;
                    break;

                // Window events
                case SDL_EVENT_WINDOW_MINIMIZED:
                    m_bStopRendering = true;
                    break;

                case SDL_EVENT_WINDOW_MAXIMIZED:
                    m_bStopRendering = false;
                    break;

                    default:
                    break;
            }

        }

        // Render draws
        draw();
    }
}

void Engine::cleanup()
{

    m_mainDeletionQueue.flush();

    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

void Engine::init_vulkan()
{
    uint32_t supportedVersion;
    vkEnumerateInstanceVersion(&supportedVersion);

    VkApplicationInfo appInfo {
        .sType  = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName =  "How to Vulkan",
        .applicationVersion = 1,
        .apiVersion = VK_MAKE_API_VERSION(0, 1, 4, 0)
    };

    uint32_t sdlExtCount{};
    char const* const* sdlExtensions {SDL_Vulkan_GetInstanceExtensions(&sdlExtCount)};
    std::vector<const char*> extensions(sdlExtensions, sdlExtensions + sdlExtCount);

#ifdef DEBUG_MODE
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    VkInstanceCreateInfo instanceInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

#ifdef DEBUG_MODE
    // Enable validation features
    // These are NOT validation layers
    std::array enabledFeatures = {
        VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT,
        VK_VALIDATION_FEATURE_ENABLE_DEBUG_PRINTF_EXT,
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
    };

    VkValidationFeaturesEXT validationFeatures{
        .sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT,
        .enabledValidationFeatureCount = enabledFeatures.size(),
        .pEnabledValidationFeatures = enabledFeatures.data()
    };

    uint32_t layerCount{};
    // Enumerate once to get count
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    // Enumerate again for data
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    std::vector<const char*> usableLayers{};
    usableLayers.reserve(m_validationLayers.size());
    // Find all the available layers we want to use if they are supported
    for (const auto& layerName: m_validationLayers)
    {
        bool layerAvailable = VrUtil::checkLayerSupport(layerName, availableLayers);
        if (!layerAvailable){
            std::cout << std::format("ENGINE: Requested layer '{}' is not available.\n", layerName);
            continue;
        }
        usableLayers.push_back(layerName);
    }

    instanceInfo.enabledLayerCount = usableLayers.size();
    instanceInfo.ppEnabledLayerNames = usableLayers.data();

    instanceInfo.pNext = &validationFeatures;


#endif

    // Create instance
    VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &m_instance));

    m_mainDeletionQueue.pushFunction([&]() {
        vkDestroyInstance(m_instance, nullptr);
    });

#ifdef DEBUG_MODE
    setupDebugMessenger();
#endif

}

void Engine::draw()
{

}

VkBool32 Engine::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData)
{
    std::cout << pCallbackData->pMessage << std::endl;
}

void Engine::setupDebugMessenger()
{
    VkDebugUtilsMessengerCreateInfoEXT messengerInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,

        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,

        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT      |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT   |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT  |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_DEVICE_ADDRESS_BINDING_BIT_EXT,

        .pfnUserCallback = debugCallback,
    };

    VK_CHECK(vkCreateDebugUtilsMessengerEXT(m_instance, &messengerInfo, nullptr, &m_debugMessenger));

    m_mainDeletionQueue.pushFunction([&]() {
       vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    });
}
