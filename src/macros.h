#pragma once

#include "vulkan/vk_enum_string_helper.h"

#include <format>

#define VK_CHECK(x) \
    do  \
    {   \
        VkResult result = x;\
        if (result != VK_SUCCESS) { \
        std::cout << std::format("VULKAN FOUND ERROR: {}", string_VkResult(result)); \
        abort();}\
} while (0)
