
#pragma once
#include <cassert>
#include <cstring>
#include <format>

#include "volk.h"

#include <string>
#include <span>
#include <functional>
#include <ranges>
#include <fstream>
#include <iostream>

namespace VrUtil
{
    struct DeletionQueue {
        std::vector<std::function<void()>> m_deletionQueue;

        void pushFunction(const std::function<void()> &function)
        {
            m_deletionQueue.push_back(function);
        }

        void flush()
        {
            for (const auto& deletor : m_deletionQueue | std::views::reverse)
            {
                deletor();
            }
            m_deletionQueue.clear();
        }
    };

    inline bool checkLayerSupport(const std::string& layerName, const std::span<VkLayerProperties>& availableLayers)
    {
        for (const auto& property : availableLayers)
        {
            if (strcmp(layerName.c_str(), property.layerName) == 0)
            {
                return true;
            }
        }

        return false;
    }

    inline std::optional<VkPhysicalDevice> getDesiredPhysicalDevice(const std::span<VkPhysicalDevice>& physDevices)
    {
        // Find first dedicated device, if not use the first
        for (const auto& device: physDevices)
        {
            VkPhysicalDeviceProperties2 properties{
                .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
            };
            vkGetPhysicalDeviceProperties2(device, &properties);
            if (properties.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
            {
                return device;
            }
        }

        if (!physDevices.empty())
        {
            return physDevices[0];
        }

        return {};
    }

    // Returns -1 error state
    inline uint32_t getDesiredQueueFamilyIndex(const std::span<VkQueueFamilyProperties2>& queueFamilies,
        VkQueueFlags desiredFamilyFlags, VkQueueFlags singleFallbackFamilyBit = VK_QUEUE_GRAPHICS_BIT)
    {
        auto findFamily = [&](const std::span<VkQueueFamilyProperties2>& families, VkQueueFlags flags) {

            for (uint32_t i = 0; i < families.size(); ++i)
            {
                families[i].sType = VK_STRUCTURE_TYPE_QUEUE_FAMILY_PROPERTIES_2;
                if (families[i].queueFamilyProperties.queueFlags & flags)
                {
                    return i;
                }
            }

            return UINT32_MAX;
        };

        if (const uint32_t familyIdx = findFamily(queueFamilies, desiredFamilyFlags) == UINT32_MAX)
        {
            // Could not find a queue family with the desired flags, try fallback
            return findFamily(queueFamilies, singleFallbackFamilyBit);
        }
        else
        {
            return familyIdx;
        }


    }

    inline std::vector<uint32_t> readFile(const std::string& filePath)
    {
        std::ifstream stream(filePath, std::ios::ate | std::ios::binary);

        if (!stream.is_open())
        {
            throw std::runtime_error(std::format("Failed to open file '{}' for shader loading.\n",filePath));
        }

        // Get file size from cursor
        std::vector<uint32_t> shaderCode(stream.tellg());

        stream.seekg(0);
        stream.read(reinterpret_cast<std::istream::char_type *>(shaderCode.data()), static_cast<std::streamsize>(shaderCode.size()));

        stream.close();

        return shaderCode;

    }

    inline VkSurfaceFormatKHR tryGetDesiredSurfaceFormat(const std::span<VkSurfaceFormatKHR>& formats,
        VkSurfaceFormatKHR desiredFormat = {VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
    {
        assert(!formats.empty() && "Found empty format while trying to obtain desired surface formats.\n");
        const auto formatIt = std::ranges::find_if(formats, [desiredFormat](const auto& format) {
            return format.format == desiredFormat.format && format.colorSpace == desiredFormat.colorSpace;
        });
        return formatIt != formats.end() ? *formatIt : formats[0];
    }

    template<typename First, typename... Links>
    inline void createVkStructureChain(Links&... link) {
    }

}
