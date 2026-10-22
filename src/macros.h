#pragma once

#include "vulkan/vk_enum_string_helper.h"

#include <format>

#define VK_CHECK(x) \
    do  \
    {   \
        VkResult VK_CHECK_RESULT = x;\
        if (VK_CHECK_RESULT != VK_SUCCESS){\
        std::cerr << std::format("VULKAN FOUND ERROR: {}\n", string_VkResult(VK_CHECK_RESULT)); \
        abort();}\
} while (0)
