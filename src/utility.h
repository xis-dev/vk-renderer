#pragma once

#include <cstring>
#include <vulkan/vulkan_core.h>

#include <string>
#include <span>

namespace VrUtil
{
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
            flush();
        }
    };
}
