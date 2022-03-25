#pragma once

#include <filesystem>
#include <map>
#include <optional>

#include <vulkan/vulkan_raii.hpp>

struct Shader
{
    vk::UniqueShaderModule shaderModule;
};

class ShaderRegistry
{
    // Using const char* might not be optimal but it is fine for now
    std::map<std::filesystem::path, Shader> vertexShaders;
    std::map<std::filesystem::path, Shader> fragmentShaders;

  public:
    enum class ErrorType
    {
        FileNotFound,
        InvalidSpriv,
        OutOfMemory,
    };

    struct Error
    {
        ErrorType type;
        union
        {
            struct
            {
                vk::Result result;
                const char* message;
            } OutOfMemory;
        };
    };

    std::optional<Error> loadVertexShader(
        const vk::UniqueDevice& device,
        const std::filesystem::path& path);
    std::optional<Error> loadFragmentShader(
        const vk::UniqueDevice& device,
        const std::filesystem::path& path);

    const Shader* getVertexShader(const std::filesystem::path& path) const;
    const Shader* getFragmentShader(const std::filesystem::path& path) const;
};