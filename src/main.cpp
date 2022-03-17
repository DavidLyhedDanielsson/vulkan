#include <cassert>
#include <iostream>

#include "vulkan.h"
#include "window.h"

int main()
{
    glfwInit();

    auto mainWindowOpt = Window::createWindow(800, 600, "Vulkan window");
    assert(mainWindowOpt.has_value());
    auto vulkanOpt = Vulkan::createVulkan();
    assert(vulkanOpt.has_value());

    auto mainWindow = std::move(mainWindowOpt.value());
    auto vulkan = std::move(vulkanOpt.value());
    while(!mainWindow.shouldClose())
    {
        mainWindow.pollEvents();
    }

    glfwTerminate();
    return 0;
}