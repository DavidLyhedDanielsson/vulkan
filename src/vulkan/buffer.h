#pragma once

#include <functional>
#include <optional>
#include <variant>
#include <vulkan/vulkan_raii.hpp>

struct Buffer
{
    class Builder
    {
        using Self = Builder;

      public:
        using MemoryTypeSelector = std::function<uint32_t(vk::MemoryRequirements)>;

        enum class ErrorType
        {
            OutOfMemory,
            CreateBuffer,
            AllocateMemory,
            NoMemoryTypeFound,
        };

        struct Error
        {
            ErrorType type;
            union
            {
                struct
                {
                    vk::Result result;
                } OutOfMemory;
                struct
                {
                    vk::Result result;
                } CreateBuffer;
                struct
                {
                    vk::Result result;
                } AllocateMemory;
                struct
                {
                    const char* message;
                } NoMemoryTypeFound;
            };
        };

        Builder(const vk::UniqueDevice&);

        Self& withSize(uint32_t);
        Self& withVertexBufferFormat();
        Self& withTransferSourceFormat(const vk::PhysicalDeviceMemoryProperties&);
        Self& withTransferDestFormat(const vk::PhysicalDeviceMemoryProperties&);
        Self& withMapFunctionality(const vk::PhysicalDeviceMemoryProperties&);

        std::variant<Buffer, Error> build() const;

      private:
        const vk::UniqueDevice& device;

        bool mapFunctionality;

        vk::BufferCreateInfo bufferInfo;
        vk::PhysicalDeviceMemoryProperties memoryProperties;
    };

    Buffer(uint32_t size, vk::UniqueBuffer&& buffer, vk::UniqueDeviceMemory&& memory);
    Buffer(Buffer&&) = default;
    Buffer& operator=(Buffer&&) = default;
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    uint32_t size;
    vk::UniqueBuffer buffer;
    vk::UniqueDeviceMemory memory;
};