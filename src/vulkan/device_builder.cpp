#include "device_builder.h"

#include "../stl_utils.h"

DeviceBuilder::DeviceBuilder(const VkInstance instance): instance(instance) {}

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
                .features = {}};
            vkGetPhysicalDeviceProperties(devices[i], &deviceInfo.properties);
            vkGetPhysicalDeviceFeatures(devices[i], &deviceInfo.features);

            if(deviceSelector)
            {
                if(deviceSelector(deviceOpt, deviceInfo))
                    deviceOpt = deviceInfo;
            }
            else
            {
                // Default selector selects any discrete GPU over any integrated GPU
                bool hasDiscrete = deviceOpt.has_value()
                                   && deviceOpt.value().properties.deviceType
                                          == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;

                if(!hasDiscrete
                   && (deviceInfo.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU
                       || deviceInfo.properties.deviceType
                              == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU))
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
    auto physicalDeviceInfo = deviceOpt.value();

    {
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDeviceInfo.device, &count, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(count);
        vkGetPhysicalDeviceQueueFamilyProperties(
            physicalDeviceInfo.device,
            &count,
            queueFamilies.data());

        for(auto [prop, index] : Index(queueFamilies))
        {
            QueueFamilyInfo info = {.index = (uint32_t)index, .properties = prop};

            if(queueFamilySelector)
            {
                if(queueFamilySelector(this->queueFamilyPropertiesOpt, info))
                    this->queueFamilyPropertiesOpt = info;
            }
            else
            {
                // Default selector selects the first one with VK_QUEUE_GRAPHICS_BIT
                if(!queueFamilyPropertiesOpt.has_value() && prop.queueFlags & VK_QUEUE_GRAPHICS_BIT)
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