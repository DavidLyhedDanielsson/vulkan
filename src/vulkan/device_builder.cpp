
#include "device_builder.h"
#include <utility>

#include "../stl_utils.h"

struct ValidatedExtension
{
    const char* name;
    bool required;
    bool found;
};

vk::Result visitExtensionProperties(
    vk::PhysicalDevice physicalDevice,
    const std::function<void(vk::ExtensionProperties)>& visitor)
{
    auto [edepRes, properties] = physicalDevice.enumerateDeviceExtensionProperties();

    if(edepRes != vk::Result::eSuccess)
        return edepRes;

    for(auto property : properties)
        visitor(property);

    return vk::Result::eSuccess;
}

DeviceBuilder::DeviceBuilder(
    const vk::UniqueInstance& instance,
    const vk::UniqueSurfaceKHR& surface)
    : instance(instance)
    , surface(surface)
{
}

DeviceBuilder& DeviceBuilder::selectDevice(DeviceSelector selector)
{
    assert(!this->gpuSelector); // TOOD: Error handling
    this->deviceSelector = std::move(selector);
    return *this;
}

DeviceBuilder& DeviceBuilder::selectGpuWithRenderSupport(
    DeviceBuilder::DeviceSelectorAfterFiltering selector)
{
    assert(!this->deviceSelector); // TOOD: Error handling
    this->gpuSelector = std::move(selector);
    return *this;
}

DeviceBuilder& DeviceBuilder::selectQueueFamily(QueueFamilySelector selector)
{
    this->queueFamilySelector = std::move(selector);
    return *this;
}

DeviceBuilder& DeviceBuilder::withRequiredExtension(const char* name)
{
    requiredExtensions.push_back(name);
    return *this;
}

std::optional<DeviceBuilder::Error> DeviceBuilder::build(SelectedConfig& config)
{
    Error error = {};

    // Always require swap chain support, whether using a custom selector or not
    requiredExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    std::optional<vk::PhysicalDevice> physicalDeviceOpt;
    {
        auto [epdRes, physicalDevices] = instance->enumeratePhysicalDevices();
        if(epdRes != vk::Result::eSuccess)
        {
            error.type = ErrorType::EnumeratePhysicalDevices;
            error.EnumeratePhysicalDevices.result = epdRes;
            return error;
        }

        for(auto physicalDevice : physicalDevices)
        {
            auto [depRes, extensionProperties] =
                physicalDevice.enumerateDeviceExtensionProperties();
            if(depRes != vk::Result::eSuccess)
            {
                error.type = ErrorType::EnumeratePhysicalDevices;
                error.EnumeratePhysicalDevices.result = depRes;
                return error;
            }

            bool hasSwapchainSupport = false;
            visitExtensionProperties(
                physicalDevice,
                [&hasSwapchainSupport](vk::ExtensionProperties prop) {
                    // deviceInfo.extensionProperties.push_back(prop);
                    if(std::strcmp(prop.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
                        hasSwapchainSupport = true;
                });

            if(!hasSwapchainSupport)
                continue;

            if(deviceSelector)
            {
                auto resultVar = deviceSelector(physicalDeviceOpt, physicalDevice);
                if(std::holds_alternative<vk::Result>(resultVar))
                {
                    error.type = ErrorType::Fatal;
                    error.Fatal.result = std::get<vk::Result>(resultVar);
                    error.Fatal.message = "Error during device selection";
                    return error;
                }
                if(std::get<bool>(resultVar))
                {
                    physicalDeviceOpt = physicalDevice;
                }
            }
            else
            {
                auto properties = physicalDevice.getProperties();
                // Default selector selects any discrete GPU over any integrated GPU, but requires
                // presentation support and VK_KHR_SWAPCHAIN_EXTENSION
                bool hasDiscrete = physicalDeviceOpt.has_value()
                                   && properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu;

                vk::Bool32 supportPresent = false;
                for(auto [queueFamilyInfo, index] :
                    Index(std::move(physicalDevice.getQueueFamilyProperties())))
                {
                    auto [gssRes, support] =
                        physicalDevice.getSurfaceSupportKHR(index, surface.get());
                    supportPresent = support;

                    if(gssRes != vk::Result::eSuccess)
                    {
                        error.type = ErrorType::Fatal;
                        error.Fatal.result = gssRes;
                        error.Fatal.message = "Error during device selection";
                        return error;
                    }
                    if(supportPresent)
                        break;
                }

                std::vector<ValidatedExtension> validatedExtensions =
                    map(requiredExtensions, [](const char* extensionName) {
                        return ValidatedExtension{
                            .name = extensionName,
                            .required = true,
                            .found = false};
                    });

                visitExtensionProperties(
                    physicalDevice,
                    [&validatedExtensions](vk::ExtensionProperties prop) {
                        for(auto [extension, index] : IndexRef(validatedExtensions))
                        {
                            if(std::strcmp(extension.name, prop.extensionName) == 0)
                            {
                                validatedExtensions[index].found = true;
                                return;
                            }
                        }
                    });

                bool hasRequiredExtensions = true;
                for(const ValidatedExtension& extension : validatedExtensions)
                {
                    if(extension.required && !extension.found)
                    {
                        hasRequiredExtensions = false;
                        break;
                    }
                }

                if(!hasDiscrete
                   && (properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu
                       || properties.deviceType == vk::PhysicalDeviceType::eIntegratedGpu)
                   && supportPresent && hasRequiredExtensions)
                {
                    if(gpuSelector)
                    {
                        auto resultVar = gpuSelector(physicalDeviceOpt, physicalDevice);
                        if(std::holds_alternative<vk::Result>(resultVar))
                        {
                            error.type = ErrorType::Fatal;
                            error.Fatal.result = std::get<vk::Result>(resultVar);
                            error.Fatal.message = "Error during device selection";
                            return error;
                        }
                        if(std::get<bool>(resultVar))
                        {
                            physicalDeviceOpt = physicalDevice;
                        }
                    }
                    else
                    {
                        physicalDeviceOpt = physicalDevice;
                    }
                }
            }
        }
    }

    if(!physicalDeviceOpt.has_value())
    {
        error.type = ErrorType::NoPhysicalDeviceFound;
        return error;
    }
    vk::PhysicalDevice physicalDevice = physicalDeviceOpt.value();

    uint32_t queueFamilyPropertiesIndex = 0; // Only valid if queueFamilyPropertiesOpt holds a value
    std::optional<vk::QueueFamilyProperties> queueFamilyPropertiesOpt;
    for(auto [prop, index] : Index(physicalDevice.getQueueFamilyProperties()))
    {
        if(queueFamilySelector)
        {
            auto resultVar = queueFamilySelector(queueFamilyPropertiesOpt, prop);
            if(std::holds_alternative<vk::Result>(resultVar))
            {
                error.type = ErrorType::Fatal;
                error.Fatal.result = std::get<vk::Result>(resultVar);
                error.Fatal.message = "Error during device selection";
                return error;
            }

            queueFamilyPropertiesOpt = prop;
            queueFamilyPropertiesIndex = index;
        }
        else
        {
            // Default selector selects the first one with VK_QUEUE_GRAPHICS_BIT and presentation
            // support
            if(!queueFamilyPropertiesOpt.has_value()
               && prop.queueFlags & vk::QueueFlagBits::eGraphics)
            {
                auto [gssRes, supportPresent] =
                    physicalDevice.getSurfaceSupportKHR(index, surface.get());

                if(gssRes != vk::Result::eSuccess)
                {
                    error.type = ErrorType::Fatal;
                    error.Fatal.result = gssRes;
                    error.Fatal.message = "Error during queue family selection";
                    return error;
                }

                if(!supportPresent)
                    continue;

                queueFamilyPropertiesOpt = prop;
                queueFamilyPropertiesIndex = index;
            }
        }
    }

    if(!queueFamilyPropertiesOpt.has_value())
    {
        error.type = ErrorType::NoQueueFamilyFound;
        return error;
    }
    auto queueFamilyProperties = queueFamilyPropertiesOpt.value();

    float queuePriority = 1.0f;
    vk::DeviceQueueCreateInfo queueInfo = {
        .queueFamilyIndex = queueFamilyPropertiesIndex,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    vk::DeviceCreateInfo deviceCreateInfo = {
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = (uint32_t)requiredExtensions.size(),
        .ppEnabledExtensionNames = requiredExtensions.data(),
        .pEnabledFeatures = nullptr,
    };

    auto [cdRes, device] = physicalDevice.createDeviceUnique(deviceCreateInfo);
    if(cdRes != vk::Result::eSuccess)
    {
        error.type = ErrorType::DeviceCreationError;
        error.DeviceCreationError.result = cdRes;
        return error;
    }

    config.device = std::move(device);
    config.queues.workQueueInfo.index = queueFamilyPropertiesIndex;
    config.queues.workQueueInfo.properties = queueFamilyProperties;
    config.physicalDevice = physicalDevice;

    return std::nullopt;
}
