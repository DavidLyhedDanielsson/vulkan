#include "shader_registry.h"

#include <variant>

#include "file_utils.h"

std::variant<vk::UniqueShaderModule, ShaderRegistry::Error> createShader(
    const vk::UniqueDevice& device,
    const std::filesystem::path& path)
{
    auto sourceOpt = FileUtils::readFile(path);
    if(!sourceOpt.has_value())
    {
        return ShaderRegistry::Error{
            .type = ShaderRegistry::ErrorType::FileNotFound,
        };
    }
    auto data = std::move(sourceOpt.value());

    auto [res, shader] = device->createShaderModuleUnique({
        .codeSize = (uint32_t)data.size(),
        .pCode = (uint32_t*)data.data(),
    });

    if(res != vk::Result::eSuccess)
    {
        if(res == vk::Result::eErrorOutOfHostMemory || res == vk::Result::eErrorOutOfDeviceMemory)
        {
            ShaderRegistry::Error error;
            error.type = ShaderRegistry::ErrorType::OutOfMemory;
            error.OutOfMemory = {.result = res, .message = "createShaderModule"};
            return error;
        }
        else
        {
            return ShaderRegistry::Error{
                .type = ShaderRegistry::ErrorType::InvalidSpriv,
            };
        }
    }

    return std::move(shader);
}

std::optional<ShaderRegistry::Error> ShaderRegistry::loadVertexShader(
    const vk::UniqueDevice& device,
    const std::filesystem::path& path)
{
    auto var = createShader(device, path);
    if(std::holds_alternative<Error>(var))
    {
        return std::get<Error>(var);
    }
    else
    {
        vertexShaders.insert(std::make_pair(
            path,
            Shader{
                .shaderModule = std::get<vk::UniqueShaderModule>(std::move(var)),
            }));
        return std::nullopt;
    }
}

std::optional<ShaderRegistry::Error> ShaderRegistry::loadFragmentShader(
    const vk::UniqueDevice& device,
    const std::filesystem::path& path)
{
    auto var = createShader(device, path);
    if(std::holds_alternative<Error>(var))
    {
        return std::get<Error>(var);
    }
    else
    {
        fragmentShaders.insert(std::make_pair(
            path,
            Shader{
                .shaderModule = std::get<vk::UniqueShaderModule>(std::move(var)),
            }));
        return std::nullopt;
    }
}
const Shader* ShaderRegistry::getVertexShader(const std::filesystem::path& path) const
{
    auto var = vertexShaders.find(path);
    if(var != vertexShaders.end())
    {
        return &var->second;
    }
    else
    {
        return nullptr;
    }
}
const Shader* ShaderRegistry::getFragmentShader(const std::filesystem::path& path) const
{
    auto var = fragmentShaders.find(path);
    if(var != fragmentShaders.end())
    {
        return &var->second;
    }
    else
    {
        return nullptr;
    }
}
