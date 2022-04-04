#pragma once

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include <functional>
#include <optional>
#include <variant>

#include "../config.h"

class DeviceBuilder
{
  public:
    enum class ErrorType
    {
        EnumeratePhysicalDevices,
        NoPhysicalDeviceFound,
        NoQueueFamilyFound,
        DeviceCreationError,
        Fatal, // Something unspecified happened but we can't continue
    };

    struct Error
    {
        ErrorType type;
        union
        {
            struct
            {
                vk::Result result;
            } EnumeratePhysicalDevices;
            struct
            {
                vk::Result result;
            } DeviceCreationError;
            struct
            {
                vk::Result result;
                const char* message;
            } Fatal;
        };
    };

    // If an error occurs, nothing should be returned. This is mainly supported because some
    // functions like vkGetPhysicalDeviceSurfaceSupportKHR might return an error
    using DeviceSelector = std::function<std::variant<bool, vk::Result>(
        const std::optional<vk::PhysicalDevice>&,
        const vk::PhysicalDevice&)>;
    using DeviceSelectorAfterFiltering = std::function<std::variant<bool, vk::Result>(
        const std::optional<vk::PhysicalDevice>&,
        const vk::PhysicalDevice&)>;
    using QueueFamilySelector = std::function<std::variant<bool, vk::Result>(
        const std::optional<vk::QueueFamilyProperties>&,
        const vk::QueueFamilyProperties&)>;

    DeviceBuilder(const vk::UniqueInstance& instance, const vk::UniqueSurfaceKHR& surface);
    DeviceBuilder& selectDevice(DeviceSelector selector);
    /**
     * Runs the default device selector to find a device with present and swapchain support, then
     * runs this function.
     *
     * Can not be used at the same time as selectDevice since this is just a filter operation after
     * DeviceSelector, so the filter can just be coded into the DeviceSelector.
     */
    DeviceBuilder& selectGpuWithRenderSupport(DeviceSelectorAfterFiltering selector);
    DeviceBuilder& selectQueueFamily(QueueFamilySelector selector);

    DeviceBuilder& withRequiredExtension(const char* name);

    std::optional<Error> build(SelectedConfig&);

  private:
    const vk::UniqueInstance& instance;
    const vk::UniqueSurfaceKHR& surface;

    std::vector<const char*> requiredExtensions;

    DeviceSelector deviceSelector;
    DeviceSelectorAfterFiltering gpuSelector;
    QueueFamilySelector queueFamilySelector;
    // SurfaceFormatSelector surfaceFormatSelector;
    // PresentModeSelector presentModeSelector;

    // std::optional<PhysicalDeviceInfo> deviceOpt;
    // std::optional<QueueFamilyInfo> queueFamilyPropertiesOpt;
};
