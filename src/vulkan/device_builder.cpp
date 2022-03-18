#include "device_builder.h"

#include "../stl_utils.h"

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

DeviceBuilder::BuildType DeviceBuilder::build()
{
    Error error = {};

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
                // presentation support
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

                if(!hasDiscrete
                   && (deviceInfo.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
                       || deviceInfo.properties.deviceType
                              == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU)
                   && supportPresent)
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
        .enabledExtensionCount = 0,
        .ppEnabledExtensionNames = nullptr,
        .pEnabledFeatures = nullptr,
    };

    VkDevice device;
    auto res = vkCreateDevice(physicalDeviceInfo.device, &deviceCreateInfo, nullptr, &device);
    if(res == VK_SUCCESS)
        return Data{
            .device = device,
            .physicalDeviceInfo = physicalDeviceInfo,
            .queueFamilyProperties = queueFamilyProperties,
        };
    else
    {
        error.type = ErrorType::DeviceCreationError;
        error.DeviceCreationError.result = res;
        return error;
    }
}