#include "engine.h"

#include "macros.h"

#include "SDL3/SDL_vulkan.h"

#define VMA_IMPLEMENTATION
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include "vk_mem_alloc.h"

#include <vector>
#include <iostream>
#include <format>

#include "mesh.h"
#include "utility.h"
#include "glm/vec3.hpp"

void transition_image_layout(VkCommandBuffer cmdBf, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout,
    VkAccessFlags2 srcAccessMask, VkAccessFlags2 dstAccessMask, VkPipelineStageFlags2 srcStageMask,
    VkPipelineStageFlags2 dstStageMask) {

    VkImageMemoryBarrier2 barrier {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = srcStageMask,
        .srcAccessMask = srcAccessMask,
        .dstStageMask = dstStageMask,
        .dstAccessMask = dstAccessMask,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };

    VkDependencyInfo dependencyInfo {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &barrier,
    };

    vkCmdPipelineBarrier2(cmdBf, &dependencyInfo);
}


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
    int ext_w, ext_h;
    SDL_GetWindowSize(m_window, &ext_w, &ext_h);
    m_windowExtent.width = ext_w;
    m_windowExtent.height = ext_h;

    init_vulkan();
    init_device();
    init_allocator();
    init_swapchain();
    init_pipelines();
    init_commands();
    init_syncObjects();
}

void Engine::run()
{
    bool running = true;
    SDL_Event e;

    int frames = 0;
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
                case SDL_EVENT_KEY_DOWN:
                    if (e.key.key == SDLK_ESCAPE) running = false;
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

    volkFinalize();
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

void Engine::init_vulkan()
{
    VK_CHECK(volkInitialize());

    uint32_t supportedVersion;
    VK_CHECK(vkEnumerateInstanceVersion(&supportedVersion));

    VkApplicationInfo appInfo {
        .sType  = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName =  "How to Vulkan",
        .applicationVersion = 1,
        .apiVersion = supportedVersion
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
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));

    // Enumerate again for data
    std::vector<VkLayerProperties> availableLayers(layerCount);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()));

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



    // Add validation features to instance create info chain
    instanceInfo.pNext = &validationFeatures;

#endif

    // Create instance
    VK_CHECK(vkCreateInstance(&instanceInfo, nullptr, &m_instance));
    volkLoadInstance(m_instance);

    m_mainDeletionQueue.pushFunction([&]() {
        vkDestroyInstance(m_instance, nullptr);
    });

#ifdef DEBUG_MODE
    setupDebugMessenger();
#endif


}

void Engine::init_device()
{
    uint32_t deviceCount{};
    VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr));
    std::vector<VkPhysicalDevice> physicalDevices(deviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(m_instance, &deviceCount, physicalDevices.data()));

    std::optional<VkPhysicalDevice> phys = VrUtil::getDesiredPhysicalDevice(physicalDevices);
    if (phys.has_value()) m_physicalDevice = phys.value();

    VkPhysicalDeviceProperties2 chosenProperties{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
    };
    vkGetPhysicalDeviceProperties2(m_physicalDevice, &chosenProperties);
    std::cout << "Chosen GPU: " << chosenProperties.properties.deviceName << std::endl;

    uint32_t queueFamilyCount{};
    vkGetPhysicalDeviceQueueFamilyProperties2(m_physicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties2> queueFamilies(queueFamilyCount);
    for (auto& f:queueFamilies)
    {
        f.sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
    }

    vkGetPhysicalDeviceQueueFamilyProperties2(m_physicalDevice, &queueFamilyCount, queueFamilies.data());

    // fallback to just graphics family if none with both
    uint32_t chosenQueueFamily = VrUtil::getDesiredQueueFamilyIndex(queueFamilies,
        VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT,
        VK_QUEUE_GRAPHICS_BIT);

    if (chosenQueueFamily == UINT32_MAX || (!SDL_Vulkan_GetPresentationSupport(m_instance, m_physicalDevice, chosenQueueFamily)))
    {
        std::cout << "ENGINE: Failed to find any sufficient queue families. Aborting\n";
        abort();
    }

    // Singular queue priority
    float queuePriority{1.0f};

    VkDeviceQueueCreateInfo queueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = chosenQueueFamily,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority
    };

    const std::vector<const char*> deviceExtensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkPhysicalDeviceVulkan12Features supportedVk12Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    };

    VkPhysicalDeviceVulkan13Features supportedVk13Features{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &supportedVk12Features
    };

    VkPhysicalDeviceFeatures2 supportedFeatures{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &supportedVk13Features
    };

    vkGetPhysicalDeviceFeatures2(m_physicalDevice, &supportedFeatures);

    VkPhysicalDeviceVulkan12Features enabledVk12Features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing = supportedVk12Features.descriptorIndexing,

        .shaderSampledImageArrayNonUniformIndexing =
            supportedVk12Features.shaderSampledImageArrayNonUniformIndexing,

        .descriptorBindingVariableDescriptorCount =
            supportedVk12Features.descriptorBindingVariableDescriptorCount,

        .runtimeDescriptorArray = supportedVk12Features.runtimeDescriptorArray,

        .bufferDeviceAddress = supportedVk12Features.bufferDeviceAddress
    };

    VkPhysicalDeviceVulkan13Features enabledVk13Features {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &enabledVk12Features,
        .synchronization2 = supportedVk13Features.synchronization2,
        .dynamicRendering = supportedVk13Features.dynamicRendering
    };

    VkPhysicalDeviceFeatures2 enabledFeatures {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &enabledVk13Features,
        .features = {
            .samplerAnisotropy = supportedFeatures.features.samplerAnisotropy
        }
    };

    VkDeviceCreateInfo deviceInfo {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &enabledFeatures,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueCreateInfo,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
    };

    VK_CHECK(vkCreateDevice(m_physicalDevice, &deviceInfo, nullptr, &m_device));
    vkGetDeviceQueue(m_device, chosenQueueFamily, 0, &m_mainQueueWithIndex.first);
    m_mainQueueWithIndex.second = chosenQueueFamily;

    m_mainDeletionQueue.pushFunction([&]() {
       vkDestroyDevice(m_device, nullptr);
    });

}

void Engine::init_allocator()
{
    VmaVulkanFunctions vkFunctions {
        .vkGetInstanceProcAddr = vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = vkGetDeviceProcAddr,
    };

    VmaAllocatorCreateInfo allocInfo {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = m_physicalDevice,
        .device = m_device,
        .pVulkanFunctions = &vkFunctions,
        .instance = m_instance
    };

    VK_CHECK(vmaCreateAllocator(&allocInfo, &m_allocator));

    VkBufferCreateInfo bufferInfo {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size =
    }
    vmaCreateBuffer(m_allocator, )
}

void Engine::init_swapchain()
{
    // Create surface with SDL
    if (!SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface))
    {
        std::cerr << "ENGINE::SDL: Failed to create vulkan surface.\n";
        abort();
    }

    VkSurfaceCapabilitiesKHR surfaceCaps{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &surfaceCaps));

    // Query surface formats, unused, simply for getting used to patterns
    uint32_t surfaceFormatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &surfaceFormatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> availableFormats(surfaceFormatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physicalDevice, m_surface, &surfaceFormatCount, availableFormats.data());

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_physicalDevice, m_surface, &presentModeCount, presentModes.data());

    VkSurfaceFormatKHR desiredFormat{VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

    VkSurfaceFormatKHR surfaceFormat = VrUtil::tryGetDesiredSurfaceFormat(availableFormats, desiredFormat);

    // Desire to use mailbox mode, fifo is similar to vsync where the display refresh takes the image at the front of the queue
    // Mailbox on the other hand, replaces images that are already queued with new ones
    VkPresentModeKHR presentMode = std::ranges::any_of(presentModes, [surfaceCaps](const auto& mode) {
        return mode == VK_PRESENT_MODE_MAILBOX_KHR && surfaceCaps.maxImageCount >= 3;
    }) ? VK_PRESENT_MODE_MAILBOX_KHR : VK_PRESENT_MODE_FIFO_KHR;


    m_swapchainExtent = {surfaceCaps.currentExtent};
    if (surfaceCaps.currentExtent.width == 0XFFFFFFFF) // Special width value for wayland
    {
        int w, h;
        SDL_GetWindowSize(m_window, &w, &h);
        m_swapchainExtent = {
            .width = static_cast<uint32_t>(w),
            .height = static_cast<uint32_t>(h)
        };
    }

    uint32_t minImageCount = std::max(3u, surfaceCaps.minImageCount);
    // If there is a capped maxImageCount, use it instead
    if (0 < surfaceCaps.maxImageCount && minImageCount < surfaceCaps.maxImageCount)
    {
       minImageCount = surfaceCaps.maxImageCount;
   }
    VkSwapchainCreateInfoKHR swapchainInfo {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = m_surface,
        .minImageCount = minImageCount,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = {m_swapchainExtent.width, m_swapchainExtent.height},
        .imageArrayLayers = 1, // Number of layers for each damage, essentially depth
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // What all images will be used for
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE, // Image sharing mode, how are images shared between queues, exclusive so 1 queue family owns, concurrent can swap between
        .preTransform = surfaceCaps.currentTransform, // Transforms we want to apply to the images, like rotation or flipping
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, // Specifies if alpha channel should be used for blending with other windows in window system
        .presentMode = presentMode,
        .clipped = true // Dont care about the colors of obscured pixels
    };

    VK_CHECK(vkCreateSwapchainKHR(m_device, &swapchainInfo, nullptr, &m_swapchain));

    uint32_t imageCount{};
    VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, nullptr));
    m_swapchainImages.resize(imageCount);
    VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swapchain, &imageCount, m_swapchainImages.data()));
    m_swapchainImageViews.resize(imageCount);
    m_swapchainResults.resize(imageCount);

    VkImageViewCreateInfo viewInfo {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = surfaceFormat.format,
        // Subresourcerange describes the image prupose and which parts should be accessed
        // Here we use the mages as colour target with no mipmaps(single level & layer, base mip level & array layer)
        .subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}
    };

    // Swizzling
    //viewInfo.components = {VK_COMPONENT_SWIZZLE_IDENTITY, ...}

    for (int i = 0; i < m_swapchainImages.size(); ++i)
    {
        viewInfo.image = m_swapchainImages[i];
        VkImageView viewOut;
        VK_CHECK(vkCreateImageView(m_device, &viewInfo, nullptr, &m_swapchainImageViews[i]));
    }

    m_mainDeletionQueue.pushFunction([&]() {

        for (const auto& imgView : m_swapchainImageViews)
        {
            vkDestroyImageView(m_device, imgView, nullptr);
        }
        
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    });
}

void Engine::init_pipelines()
{
    // TODO: move to pipeline builder
    // Vertex attributes & bindings
    VkVertexInputBindingDescription bindingDescriptions[] = {
        {.binding = 0, .stride = sizeof(Vertex), .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}
    };

    VkVertexInputAttributeDescription attributeDescriptions[] = {
        {.location = 0, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, position)},
        {.location = 1, .binding = 0, .format = VK_FORMAT_R32G32B32_SFLOAT, .offset = offsetof(Vertex, color)},
    };

    VkPipelineVertexInputStateCreateInfo vertexInput {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = bindingDescriptions,
        .vertexAttributeDescriptionCount = 2,
        .pVertexAttributeDescriptions = attributeDescriptions,
    };

    // Input assembly
    VkPipelineInputAssemblyStateCreateInfo inputAssembly {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST
    };

    // Rasterization state
    // Cull mode & polygon mode will be dynamic but set for now
    VkPipelineRasterizationStateCreateInfo rasterizationState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .lineWidth = 1.0f,
    };

    // Multisampling state, disable for now
    VkPipelineMultisampleStateCreateInfo multisampleState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
    };

    // Depth/Stencil state
    VkPipelineDepthStencilStateCreateInfo dsInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f,
    };

    VkPipelineColorBlendAttachmentState colorAttachmentState {

        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |VK_COLOR_COMPONENT_G_BIT |VK_COLOR_COMPONENT_B_BIT |VK_COLOR_COMPONENT_A_BIT,
    };
    // Colour blend state
    VkPipelineColorBlendStateCreateInfo colorBlendState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &colorAttachmentState,
    };

    VkDynamicState enabledDynamicState[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    // Dynamic states
    VkPipelineDynamicStateCreateInfo dynamicState {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = enabledDynamicState,
    };


    VkShaderModule vertexShader;
    VkShaderModule fragmentShader;

    const auto vertexShaderCode = VrUtil::readFile("assets/shaders/triangle.vert.spv");

    VkShaderModuleCreateInfo vertexModuleInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = vertexShaderCode.size(),
        .pCode = vertexShaderCode.data(),
    };

    vkCreateShaderModule(m_device, &vertexModuleInfo, nullptr, &vertexShader);

    VkPipelineShaderStageCreateInfo vertexStageInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = (VkShaderStageFlagBits) (VK_SHADER_STAGE_VERTEX_BIT),
        .module = vertexShader,
        .pName = "main"
    };

    const auto fragmentCode = VrUtil::readFile("assets/shaders/triangle.frag.spv");

    VkShaderModuleCreateInfo fragmentModuleInfo{
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = fragmentCode.size(),
        .pCode = fragmentCode.data(),
    };

    vkCreateShaderModule(m_device, &fragmentModuleInfo, nullptr, &fragmentShader);

    VkPipelineShaderStageCreateInfo fragmentStageInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = (VkShaderStageFlagBits) (VK_SHADER_STAGE_FRAGMENT_BIT),
        .module = fragmentShader,
        .pName = "main",
    };

    const VkPipelineShaderStageCreateInfo shaderStages[] = {
        vertexStageInfo,
        fragmentStageInfo
    };

    VkViewport viewport {
        .x = 0.0f, .y = 0.0f,
        .width = static_cast<float>(m_swapchainExtent.width),
        .height = static_cast<float>(m_swapchainExtent.height),
        .minDepth = dsInfo.minDepthBounds,
        .maxDepth = dsInfo.maxDepthBounds
    };

    VkRect2D scissor {
        VkOffset2D{},
        m_swapchainExtent
    };

    VkPipelineViewportStateCreateInfo viewportState{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };


    VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    };
    // Layout creation
    VkPipelineLayoutCreateInfo layoutInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
    };

    // Empty pipeline for now
    VK_CHECK(vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_triangleLayout));

    VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;

    std::vector<VkFormat> depthFormats {
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT,
    };
    VkFormat depthFormat {VK_FORMAT_UNDEFINED};

    for (const auto& format : depthFormats) {
        VkFormatProperties2 formatProperties {
            .sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2
        };

        vkGetPhysicalDeviceFormatProperties2(m_physicalDevice, format, &formatProperties);

        if (formatProperties.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            depthFormat = format;
            break;
        }
    }

    VkPipelineRenderingCreateInfo renderInfo {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &colorFormat,
        .depthAttachmentFormat = depthFormat,
    };

    // Viewport state will be dynamic, so leave null
    // No tessellation, leave null
    VkGraphicsPipelineCreateInfo pipelineInfo {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &renderInfo,
        .stageCount = 2,
        .pStages = shaderStages,
        .pVertexInputState = &vertexInput,
        .pInputAssemblyState = &inputAssembly,
        .pViewportState = &viewportState,
        .pRasterizationState = &rasterizationState,
        .pMultisampleState = &multisampleState,
        .pDepthStencilState = &dsInfo,
        .pColorBlendState = &colorBlendState,
        .pDynamicState = &dynamicState,
        .layout = m_triangleLayout,
    };

    VK_CHECK(vkCreateGraphicsPipelines(m_device, nullptr, 1, &pipelineInfo, nullptr, &m_trianglePipeline));

    vkDestroyShaderModule(m_device, vertexShader, nullptr);
    vkDestroyShaderModule(m_device, fragmentShader, nullptr);

    m_mainDeletionQueue.pushFunction([&]() {
        vkDestroyPipelineLayout(m_device, m_triangleLayout, nullptr);
        vkDestroyPipeline(m_device, m_trianglePipeline, nullptr);
    });


}

void Engine::init_commands() {

    // Create command pool
    VkCommandPoolCreateInfo commandPoolInfo {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        // Allow any allocated command buffer to individually be set to initial state
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = m_mainQueueWithIndex.second,
    };

    VK_CHECK(vkCreateCommandPool(m_device, &commandPoolInfo, nullptr, &m_commandPool));


     VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
         .commandPool = m_commandPool,
         .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
         .commandBufferCount = 1
     };

    vkAllocateCommandBuffers(m_device, &allocInfo, &m_commandBuffer);

    // Command buffer recording

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO
    };

    vkBeginCommandBuffer(m_commandBuffer, &beginInfo);

}

void Engine::init_syncObjects() {

    VkFenceCreateInfo fenceInfo {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    vkCreateFence(m_device, &fenceInfo, nullptr, &m_drawFence);

    // No pNext of VkSemaphoreTYpeCreateInfo, defaults to binary semaphore
    VkSemaphoreCreateInfo semaphoreInfo {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_presentSemaphore);
    vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderSemaphore);

    m_mainDeletionQueue.pushFunction([&]() {
        vkDestroySemaphore(m_device, m_renderSemaphore, nullptr);
        vkDestroySemaphore(m_device, m_presentSemaphore, nullptr);

        vkDestroyFence(m_device, m_drawFence, nullptr);
    });

}

void Engine::draw()
{
    auto fenceResult = vkWaitForFences(m_device, 1, &m_drawFence, VK_TRUE, UINT64_MAX);
    if (fenceResult != VK_SUCCESS) {
        throw std::runtime_error("Failed to wait for fences.\n");
    }

    vkResetFences(m_device, 1, &m_drawFence);

    uint32_t imageIdx{};
    VK_CHECK(vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, m_renderSemaphore, nullptr, &imageIdx));


    VkClearValue clear {
        .color = {0.0f, 0.0f, 0.0f, 0.0f},
    };

    VkRenderingAttachmentInfo attachmentInfo {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageView = m_swapchainImageViews[imageIdx],
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clear,
    };

    VkRenderingInfo renderInfo {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachmentInfo,
    };

    vkCmdBeginRendering(m_commandBuffer, &renderInfo);

    vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_trianglePipeline);

    VkViewport viewport {
        .width = static_cast<float>(m_windowExtent.width),
        .height = static_cast<float>(m_windowExtent.height)
    };

    VkRect2D scissor {
        .extent = m_windowExtent,
    };

    vkCmdSetViewport(m_commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor);

    vkCmdBindVertexBuffers(m_commandBuffer, 0, 1,)

    vkCmdDraw(m_commandBuffer, 3, 1, 0, 0);

    vkCmdEndRendering(m_commandBuffer);


    // Submit command buffer
    VkPipelineStageFlags waitDestMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const VkSubmitInfo submitInfo {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &m_presentSemaphore,
        .pWaitDstStageMask = &waitDestMask,
        .commandBufferCount = 1,
        .pCommandBuffers = &m_commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &m_renderSemaphore,
    };

    vkQueueSubmit(m_mainQueueWithIndex.first, 1, &submitInfo, m_drawFence);

    // Present the result of the swapchain
    VkPresentInfoKHR presentInfo {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &m_renderSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &m_swapchain,
        .pImageIndices = &imageIdx,
        // Only using 1 swapchain but want to know i have the option to
        .pResults = m_swapchainResults.data()
    };

    vkQueuePresentKHR(m_mainQueueWithIndex.first, &presentInfo);

    for (const auto& result : m_swapchainResults) {
        VK_CHECK(result);
    }

}

#ifdef DEBUG_MODE
VkBool32 Engine::debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void *pUserData)
{
    std::cout << pCallbackData->pMessage << std::endl;

    return false;
}

void Engine::setupDebugMessenger()
{

    VkDebugUtilsMessengerCreateInfoEXT messengerInfo{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,

        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,

        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT      |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT   |
                       VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,

        .pfnUserCallback = debugCallback,
    };

    VK_CHECK(vkCreateDebugUtilsMessengerEXT(m_instance, &messengerInfo, nullptr, &m_debugMessenger));

    m_mainDeletionQueue.pushFunction([&]() {
       vkDestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
    });
}

#endif