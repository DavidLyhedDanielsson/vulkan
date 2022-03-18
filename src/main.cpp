#include <cassert>
#include <iostream>

#include "vulkan/vulkan.h"
#include "window.h"

int main()
{
    glfwInit();

    auto mainWindowOpt = Window::createWindow(800, 600, "Vulkan window");
    assert(mainWindowOpt.has_value());
    Window mainWindow = std::move(mainWindowOpt.value());

    auto vulkanVar = Vulkan::createVulkan(mainWindow.getWindowHandle());
    assert(!std::holds_alternative<Vulkan::Error>(vulkanVar));
    auto vulkan = std::get<Vulkan>(std::move(vulkanVar));

    while(!mainWindow.shouldClose())
    {
        mainWindow.pollEvents();
    }

    glfwTerminate();
    return 0;
}