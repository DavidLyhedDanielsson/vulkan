#include "buffer.h"

using Builder = Buffer::Builder;

Builder::Builder(const vk::UniqueDevice& device): device(device), mapFunctionality(false)
{
    this->bufferInfo.sharingMode = vk::SharingMode::eExclusive;
}

Builder& Builder::withSize(uint32_t size)
{
    bufferInfo.size = size;
    return *this;
}

Builder& Builder::withVertexBufferFormat()
{
    bufferInfo.usage |= vk::BufferUsageFlagBits::eVertexBuffer;
    return *this;
}

Builder::Self& Buffer::Builder::withTransferSourceFormat(
    const vk::PhysicalDeviceMemoryProperties& memoryProperties)
{
    bufferInfo.usage |= vk::BufferUsageFlagBits::eTransferSrc;
    this->memoryProperties = memoryProperties;
    return *this;
}

Builder::Self& Buffer::Builder::withTransferDestFormat(
    const vk::PhysicalDeviceMemoryProperties& memoryProperties)
{
    bufferInfo.usage |= vk::BufferUsageFlagBits::eTransferDst;
    this->memoryProperties = memoryProperties;
    return *this;
}

Builder& Builder::withMapFunctionality(const vk::PhysicalDeviceMemoryProperties& memoryProperties)
{
    mapFunctionality = true;
    this->memoryProperties = memoryProperties;
    return *this;
}

std::variant<Buffer, Builder::Error> Builder::build() const
{
    Builder::Error error = {};

    auto [cbRes, buffer] = device->createBufferUnique(bufferInfo);
    if(cbRes == vk::Result::eErrorOutOfHostMemory || cbRes == vk::Result::eErrorOutOfDeviceMemory)
    {
        error.type = Builder::ErrorType::OutOfMemory;
        error.OutOfMemory.result = cbRes;
        return error;
    }
    else if(cbRes != vk::Result::eSuccess)
    {
        error.type = Builder::ErrorType::CreateBuffer;
        error.CreateBuffer.result = cbRes;
        return error;
    }

    vk::MemoryRequirements memoryRequirements = device->getBufferMemoryRequirements(buffer.get());
    uint32_t memoryIndex; // What is the default here..?
    auto memoryProperties = this->memoryProperties;

    std::optional<uint32_t> memoryIndexOpt = 0;
    for(uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        vk::MemoryPropertyFlags required;
        if(mapFunctionality)
        {
            required |= vk::MemoryPropertyFlagBits::eHostVisible
                        | vk::MemoryPropertyFlagBits::eHostCoherent;
        }
        else if(bufferInfo.usage & vk::BufferUsageFlagBits::eTransferDst)
        {
            required |= vk::MemoryPropertyFlagBits::eDeviceLocal;
        }
        if(memoryRequirements.memoryTypeBits & (1 << i)
           && (memoryProperties.memoryTypes[i].propertyFlags & required))
        {
            memoryIndexOpt = i;
            break;
        }
    }
    if(!memoryIndexOpt)
    {
        error.type = Builder::ErrorType::NoMemoryTypeFound;
        error.NoMemoryTypeFound.message = "Looking for HostVisible and HostCoherent";
        return error;
    }
    memoryIndex = memoryIndexOpt.value();

    vk::MemoryAllocateInfo allocateInfo = {
        .allocationSize = memoryRequirements.size,
        .memoryTypeIndex = memoryIndex,
    };
    auto [amRes, memory] = device->allocateMemoryUnique(allocateInfo);
    if(amRes == vk::Result::eErrorOutOfHostMemory || amRes == vk::Result::eErrorOutOfDeviceMemory)
    {
        error.type = Builder::ErrorType::OutOfMemory;
        error.OutOfMemory.result = amRes;
        return error;
    }
    else if(amRes != vk::Result::eSuccess)
    {
        error.type = Builder::ErrorType::AllocateMemory;
        error.AllocateMemory.result = amRes;
        return error;
    }

    return Buffer(memoryRequirements.size, std::move(buffer), std::move(memory));
}

Buffer::Buffer(uint32_t size, vk::UniqueBuffer&& buffer, vk::UniqueDeviceMemory&& memory)
    : size(size)
    , buffer(std::move(buffer))
    , memory(std::move(memory))
{
}
