#pragma once

#include <vulkan/vulkan.hpp>

struct Config
{
    uint32_t resolutionWidth;
    uint32_t resolutionHeight;
    vk::Format backbufferFormat;
    vk::SampleCountFlagBits
        sampleCount; // This is a bit unclear and should probably be changed to an enum
    uint32_t backbufferCount;
};