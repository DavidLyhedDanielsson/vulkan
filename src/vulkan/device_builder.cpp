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
    {
        uint32_t count = 0;
        vkEnumeratePhysicalDevices(instance, &count, nullptr);

        std::vector<VkPhysicalDevice> devices(count);
        auto res = vkEnumeratePhysicalDevices(instance, &count, devices.data());
        assert(res == VK_SUCCESS);

        for(uint32_t i = 0; i < count; ++i)
        {
            DeviceInfo deviceInfo = {.device = devices[i], .properties = {}, .features = {}};
            vkGetPhysicalDeviceProperties(devices[i], &deviceInfo.properties);
            vkGetPhysicalDeviceFeatures(devices[i], &deviceInfo.features);

            if(deviceSelector(deviceOpt, deviceInfo))
                deviceOpt = deviceInfo;
        }
    }

    if(!deviceOpt.has_value())
        return BuildType(DeviceError{});
    auto physicalDevice = deviceOpt.value().device;

    {
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(count);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &count, queueFamilies.data());

        for(auto [prop, index] : Index(queueFamilies))
        {
            QueueFamilyInfo info = {.index = (uint32_t)index, .properties = prop};
            if(queueFamilySelector(this->queueFamilyPropertiesOpt, info))
                this->queueFamilyPropertiesOpt = info;
        }
    }

    if(!queueFamilyPropertiesOpt.has_value())
        return BuildType(DeviceError{});
    auto queueFamilyIndex = queueFamilyPropertiesOpt.value().index;

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .queueFamilyIndex = queueFamilyIndex,
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
    auto res = vkCreateDevice(physicalDevice, &deviceCreateInfo, nullptr, &device);
    if(res == VK_SUCCESS)
        return BuildType(device);
    else
        return BuildType(DeviceError{});
}