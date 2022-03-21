#include "device_builder.h"

#include "../stl_utils.h"

struct ValidatedExtension
{
    const char* name;
    bool required;
    bool found;
};

VkResult visitExtensionProperties(
    VkPhysicalDevice physicalDevice,
    std::function<void(VkExtensionProperties)> visitor)
{
    uint32_t count = 0;
    auto res = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, nullptr);
    if(res != VK_SUCCESS)
        return res;

    std::vector<VkExtensionProperties> properties(count);
    res = vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &count, properties.data());
    if(res != VK_SUCCESS)
        return res;

    for(uint32_t i = 0; i < count; ++i)
        visitor(properties[i]);

    return VK_SUCCESS;
}

DeviceBuilder::DeviceBuilder(const VkInstance instance, const VkSurfaceKHR surface)
    : instance(instance)
    , surface(surface)
{
}

DeviceBuilder& DeviceBuilder::selectDevice(DeviceSelector selector)
{
    this->deviceSelector = selector;
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
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance, &count, nullptr);

        std::vector<VkPhysicalDevice> devices(count);
        auto res = vkEnumeratePhysicalDevices(instance, &count, devices.data());
        if(res != VK_SUCCESS)
        {
            error.type = ErrorType::EnumeratePhysicalDevices;
            error.EnumeratePhysicalDevices.result = res;
            return error;
        }

        for(uint32_t i = 0; i < count; ++i)
        {
            PhysicalDeviceInfo deviceInfo = {
                .device = devices[i],
                .properties = {},
                .features = {},
                .queueFamilyProperties = {},
                .extensionProperties = {},
                .surfaceFormats = {},
                .presentModes = {},
                .surfaceCapabilities = {},
            };
            vkGetPhysicalDeviceProperties(devices[i], &deviceInfo.properties);
            vkGetPhysicalDeviceFeatures(devices[i], &deviceInfo.features);

            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(deviceInfo.device, &queueFamilyCount, nullptr);
            deviceInfo.queueFamilyProperties.resize(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(
                deviceInfo.device,
                &queueFamilyCount,
                deviceInfo.queueFamilyProperties.data());

            bool hasSwapchainSupport = false;
            visitExtensionProperties(
                deviceInfo.device,
                [&deviceInfo, &hasSwapchainSupport](VkExtensionProperties prop) {
                    deviceInfo.extensionProperties.push_back(prop);
                    if(std::strcmp(prop.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0)
                        hasSwapchainSupport = true;
                });

            if(!hasSwapchainSupport)
                continue;

            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                deviceInfo.device,
                surface,
                &deviceInfo.surfaceCapabilities);

            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(deviceInfo.device, surface, &formatCount, nullptr);
            deviceInfo.surfaceFormats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(
                deviceInfo.device,
                surface,
                &formatCount,
                deviceInfo.surfaceFormats.data());

            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                deviceInfo.device,
                surface,
                &presentModeCount,
                nullptr);
            deviceInfo.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                deviceInfo.device,
                surface,
                &presentModeCount,
                deviceInfo.presentModes.data());

            if(deviceSelector)
            {
                auto resultVar = deviceSelector(deviceOpt, deviceInfo);
                if(std::holds_alternative<VkResult>(resultVar))
                {
                    error.type = ErrorType::Fatal;
                    error.Fatal.result = std::get<VkResult>(resultVar);
                    error.Fatal.message = "Error during device selection";
                    return error;
                }
                deviceOpt = deviceInfo;
            }
            else
            {
                // Default selector selects any discrete GPU over any integrated GPU, but requires
                // presentation support and VK_KHR_SWAPCHAIN_EXTENSION
                bool hasDiscrete = deviceOpt.has_value()
                                   && deviceOpt.value().properties.deviceType
                                          == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

                VkBool32 supportPresent = false;
                for(auto [queueFamilyInfo, index] : Index(deviceInfo.queueFamilyProperties))
                {
                    auto res = vkGetPhysicalDeviceSurfaceSupportKHR(
                        deviceInfo.device,
                        index,
                        surface,
                        &supportPresent);

                    if(res != VK_SUCCESS)
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
                    [&validatedExtensions](VkExtensionProperties prop) {
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
                   && (deviceInfo.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
                       || deviceInfo.properties.deviceType
                              == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
                   && supportPresent && hasRequiredExtensions)
                {
                    deviceOpt = deviceInfo;
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
            if(std::holds_alternative<VkResult>(resultVar))
            {
                error.type = ErrorType::Fatal;
                error.Fatal.result = std::get<VkResult>(resultVar);
                error.Fatal.message = "Error during device selection";
                return error;
            }

            this->queueFamilyPropertiesOpt = info;
        }
        else
        {
            // Default selector selects the first one with VK_QUEUE_GRAPHICS_BIT and presentation
            // support
            if(!queueFamilyPropertiesOpt.has_value() && prop.queueFlags & VK_QUEUE_GRAPHICS_BIT)
            {
                VkBool32 supportPresent = false;
                auto res = vkGetPhysicalDeviceSurfaceSupportKHR(
                    physicalDeviceInfo.device,
                    index,
                    surface,
                    &supportPresent);

                if(res != VK_SUCCESS)
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
    VkDeviceQueueCreateInfo queueInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = queueFamilyProperties.index,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };

    VkDeviceCreateInfo deviceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queueInfo,
        .enabledLayerCount = 0,
        .ppEnabledLayerNames = nullptr,
        .enabledExtensionCount = (uint32_t)requiredExtensions.size(),
        .ppEnabledExtensionNames = requiredExtensions.data(),
        .pEnabledFeatures = nullptr,
    };

    VkDevice device;
    auto res = vkCreateDevice(physicalDeviceInfo.device, &deviceCreateInfo, nullptr, &device);
    if(res != VK_SUCCESS)
    {
        error.type = ErrorType::DeviceCreationError;
        error.DeviceCreationError.result = res;
        return error;
    }

    auto defaultSurfaceFormatSelector =
        [](const PhysicalDeviceInfo& info) -> std::optional<VkSurfaceFormatKHR> {
        auto iter =
            std::find_if(entire_collection(info.surfaceFormats), [](VkSurfaceFormatKHR format) {
                return format.format == VK_FORMAT_B8G8R8A8_SRGB;
            });

        if(iter == info.surfaceFormats.end())
            return std::nullopt;
        else
            return *iter;
    };
    std::optional<VkSurfaceFormatKHR> surfaceFormatOpt =
        (this->surfaceFormatSelector ? this->surfaceFormatSelector
                                     : defaultSurfaceFormatSelector)(physicalDeviceInfo);
    if(!surfaceFormatOpt.has_value())
    {
        error.type = ErrorType::NoSurfaceFormatFound;
        return error;
    }
    VkSurfaceFormatKHR surfaceFormat = surfaceFormatOpt.value();

    auto defaultPresentModeSelector =
        [](const PhysicalDeviceInfo&) -> std::optional<VkPresentModeKHR> {
        return VK_PRESENT_MODE_FIFO_KHR;
    };
    std::optional<VkPresentModeKHR> presentModeOpt =
        (this->presentModeSelector ? this->presentModeSelector
                                   : defaultPresentModeSelector)(physicalDeviceInfo);
    if(!presentModeOpt.has_value())
    {
        error.type = ErrorType::NoPresentModeFound;
        return error;
    }
    VkPresentModeKHR presentMode = presentModeOpt.value();

    VkSwapchainCreateInfoKHR swapChainCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .surface = surface,
        .minImageCount = physicalDeviceInfo.surfaceCapabilities.minImageCount + 1,
        .imageFormat = surfaceFormat.format,
        .imageColorSpace = surfaceFormat.colorSpace,
        .imageExtent = physicalDeviceInfo.surfaceCapabilities.currentExtent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
        .preTransform = physicalDeviceInfo.surfaceCapabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };
    VkSwapchainKHR swapChain;
    res = vkCreateSwapchainKHR(device, &swapChainCreateInfo, nullptr, &swapChain);

    if(res != VK_SUCCESS)
    {
        error.type = ErrorType::SwapChainCreationError;
        error.SwapChainCreationError.result = res;
        return error;
    }

    return Data{
        .device = device,
        .swapChain = swapChain,
        .physicalDeviceInfo = physicalDeviceInfo,
        .queueFamilyProperties = queueFamilyProperties,
    };
}