#include "device_builder.h"

#include "../stl_utils.h"

struct ValidatedExtension
{
    const char* name;
    bool required;
    bool found;
};

vk::Result visitExtensionProperties(
    vk::PhysicalDevice physicalDevice,
    std::function<void(vk::ExtensionProperties)> visitor)
{
    auto [res, properties] = physicalDevice.enumerateDeviceExtensionProperties();

    if(res != vk::Result::eSuccess)
        return res;

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
    this->deviceSelector = selector;
    return *this;
}

DeviceBuilder& DeviceBuilder::selectGpuWithRenderSupport(
    DeviceBuilder::DeviceSelectorAfterFiltering selector)
{
    assert(!this->deviceSelector); // TOOD: Error handling
    this->gpuSelector = selector;
    return *this;
}

DeviceBuilder& DeviceBuilder::selectQueueFamily(QueueFamilySelector selector)
{
    this->queueFamilySelector = selector;
    return *this;
}

DeviceBuilder& DeviceBuilder::selectSurfaceFormat(SurfaceFormatSelector selector)
{
    this->surfaceFormatSelector = selector;
    return *this;
}

DeviceBuilder& DeviceBuilder::selectPresentMode(PresentModeSelector selector)
{
    this->presentModeSelector = selector;
    return *this;
}

DeviceBuilder& DeviceBuilder::withRequiredExtension(const char* name)
{
    requiredExtensions.push_back(name);
    return *this;
}

DeviceBuilder::BuildType DeviceBuilder::build()
{
    Error error = {};

    // Always require swap chain support, whether using a custom selector or not
    requiredExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);

    {
        auto [res, physicalDevices] = instance->enumeratePhysicalDevices();
        if(res != vk::Result::eSuccess)
        {
            error.type = ErrorType::EnumeratePhysicalDevices;
            error.EnumeratePhysicalDevices.result = res;
            return error;
        }

        for(auto device : physicalDevices)
        {
            auto [depRes, extensionProperties] = device.enumerateDeviceExtensionProperties();
            if(depRes != vk::Result::eSuccess)
            {
                error.type = ErrorType::EnumeratePhysicalDevices;
                error.EnumeratePhysicalDevices.result = depRes;
                return error;
            }
            auto [sfRes, surfaceFormats] = device.getSurfaceFormatsKHR(surface.get());
            if(sfRes != vk::Result::eSuccess)
            {
                error.type = ErrorType::NoSurfaceFormatFound;
                error.NoSurfaceFormatFound.result = sfRes;
                return error;
            }
            auto [pmRes, presentModes] = device.getSurfacePresentModesKHR(surface.get());
            if(pmRes != vk::Result::eSuccess)
            {
                error.type = ErrorType::NoPresentModeFound;
                error.NoPresentModeFound.result = pmRes;
                return error;
            }
            auto [scRes, surfaceCapabilities] = device.getSurfaceCapabilitiesKHR(surface.get());
            if(scRes != vk::Result::eSuccess)
            {
                error.type = ErrorType::NoSurfaceCapabilitiesFound;
                error.NoSurfaceCapabilitiesFound.result = scRes;
                return error;
            }

            PhysicalDeviceInfo deviceInfo = {
                .device = device,
                .properties = device.getProperties(),
                .features = device.getFeatures(),
                .queueFamilyProperties = device.getQueueFamilyProperties(),
                .extensionProperties = extensionProperties,
                .surfaceFormats = surfaceFormats,
                .presentModes = presentModes,
                .surfaceCapabilities = surfaceCapabilities,
            };

            bool hasSwapchainSupport = false;
            visitExtensionProperties(
                deviceInfo.device,
                [&deviceInfo, &hasSwapchainSupport](vk::ExtensionProperties prop) {
                    deviceInfo.extensionProperties.push_back(prop);
                    if(std::strcmp(prop.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
                        hasSwapchainSupport = true;
                });

            if(!hasSwapchainSupport)
                continue;

            if(deviceSelector)
            {
                auto resultVar = deviceSelector(deviceOpt, deviceInfo);
                if(std::holds_alternative<vk::Result>(resultVar))
                {
                    error.type = ErrorType::Fatal;
                    error.Fatal.result = std::get<vk::Result>(resultVar);
                    error.Fatal.message = "Error during device selection";
                    return error;
                }
                if(std::get<bool>(resultVar))
                {
                    deviceOpt = deviceInfo;
                }
            }
            else
            {
                // Default selector selects any discrete GPU over any integrated GPU, but requires
                // presentation support and VK_KHR_SWAPCHAIN_EXTENSION
                bool hasDiscrete = deviceOpt.has_value()
                                   && deviceOpt.value().properties.deviceType
                                          == vk::PhysicalDeviceType::eDiscreteGpu;

                vk::Bool32 supportPresent = false;
                for(auto [queueFamilyInfo, index] : Index(deviceInfo.queueFamilyProperties))
                {
                    auto [res, support] =
                        deviceInfo.device.getSurfaceSupportKHR(index, surface.get());
                    supportPresent = support;

                    if(res != vk::Result::eSuccess)
                    {
                        error.type = ErrorType::Fatal;
                        error.Fatal.result = res;
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
                    deviceInfo.device,
                    [&validatedExtensions](vk::ExtensionProperties prop) {
                        for(auto [extension, index] : Index(validatedExtensions))
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
                   && (deviceInfo.properties.deviceType == vk::PhysicalDeviceType::eDiscreteGpu
                       || deviceInfo.properties.deviceType
                              == vk::PhysicalDeviceType::eIntegratedGpu)
                   && supportPresent && hasRequiredExtensions)
                {
                    if(gpuSelector)
                    {
                        auto resultVar = gpuSelector(deviceOpt, deviceInfo);
                        if(std::holds_alternative<vk::Result>(resultVar))
                        {
                            error.type = ErrorType::Fatal;
                            error.Fatal.result = std::get<vk::Result>(resultVar);
                            error.Fatal.message = "Error during device selection";
                            return error;
                        }
                        if(std::get<bool>(resultVar))
                        {
                            deviceOpt = deviceInfo;
                        }
                    }
                    else
                    {
                        deviceOpt = deviceInfo;
                    }
                }
            }
        }
    }

    if(!deviceOpt.has_value())
    {
        error.type = ErrorType::NoPhysicalDeviceFound;
        return error;
    }
    PhysicalDeviceInfo physicalDeviceInfo = deviceOpt.value();

    for(auto [prop, index] : Index(physicalDeviceInfo.queueFamilyProperties))
    {
        QueueFamilyInfo info = {.index = (uint32_t)index, .properties = prop};

        if(queueFamilySelector)
        {
            auto resultVar = queueFamilySelector(this->queueFamilyPropertiesOpt, info);
            if(std::holds_alternative<vk::Result>(resultVar))
            {
                error.type = ErrorType::Fatal;
                error.Fatal.result = std::get<vk::Result>(resultVar);
                error.Fatal.message = "Error during device selection";
                return error;
            }

            this->queueFamilyPropertiesOpt = info;
        }
        else
        {
            // Default selector selects the first one with VK_QUEUE_GRAPHICS_BIT and presentation
            // support
            if(!queueFamilyPropertiesOpt.has_value()
               && prop.queueFlags & vk::QueueFlagBits::eGraphics)
            {
                auto [res, supportPresent] =
                    physicalDeviceInfo.device.getSurfaceSupportKHR(index, surface.get());

                if(res != vk::Result::eSuccess)
                {
                    error.type = ErrorType::Fatal;
                    error.Fatal.result = res;
                    error.Fatal.message = "Error during queue family selection";
                    return error;
                }

                if(!supportPresent)
                    continue;

                this->queueFamilyPropertiesOpt = info;
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
        .queueFamilyIndex = queueFamilyProperties.index,
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

    auto [cdRes, device] = physicalDeviceInfo.device.createDeviceUnique(deviceCreateInfo);
    if(cdRes != vk::Result::eSuccess)
    {
        error.type = ErrorType::DeviceCreationError;
        error.DeviceCreationError.result = cdRes;
        return error;
    }

    auto defaultSurfaceFormatSelector =
        [](const PhysicalDeviceInfo& info) -> std::optional<vk::SurfaceFormatKHR> {
        auto iter =
            std::find_if(entire_collection(info.surfaceFormats), [](vk::SurfaceFormatKHR format) {
                return format.format == vk::Format::eB8G8R8A8Srgb;
            });

        if(iter == info.surfaceFormats.end())
            return std::nullopt;
        else
            return *iter;
    };
    std::optional<vk::SurfaceFormatKHR> surfaceFormatOpt =
        (this->surfaceFormatSelector ? this->surfaceFormatSelector
                                     : defaultSurfaceFormatSelector)(physicalDeviceInfo);
    if(!surfaceFormatOpt.has_value())
    {
        error.type = ErrorType::NoSurfaceFormatFound;
        return error;
    }
    vk::SurfaceFormatKHR surfaceFormat = surfaceFormatOpt.value();

    auto defaultPresentModeSelector =
        [](const PhysicalDeviceInfo&) -> std::optional<vk::PresentModeKHR> {
        return vk::PresentModeKHR::eFifo;
    };
    std::optional<vk::PresentModeKHR> presentModeOpt =
        (this->presentModeSelector ? this->presentModeSelector
                                   : defaultPresentModeSelector)(physicalDeviceInfo);
    if(!presentModeOpt.has_value())
    {
        error.type = ErrorType::NoPresentModeFound;
        return error;
    }
    vk::PresentModeKHR presentMode = presentModeOpt.value();

    vk::SwapchainCreateInfoKHR swapChainCreateInfo = {
        .surface = surface.get(),
        .minImageCount = physicalDeviceInfo.surfaceCapabilities.minImageCount + 1,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = physicalDeviceInfo.surfaceCapabilities.currentExtent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = physicalDeviceInfo.surfaceCapabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    auto [csRes, swapChain] = device->createSwapchainKHRUnique(swapChainCreateInfo);
    if(csRes != vk::Result::eSuccess)
    {
        error.type = ErrorType::SwapChainCreationError;
        error.SwapChainCreationError.result = csRes;
        return error;
    }

    return Data{
        .device = std::move(device),
        .swapChain = std::move(swapChain),
        .physicalDeviceInfo = physicalDeviceInfo,
        .queueFamilyProperties = queueFamilyProperties,
    };
}
