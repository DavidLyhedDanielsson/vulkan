#include <assert.h>
#include <iostream>

#include "window.h"

int main()
{
    glfwInit();

    auto mainWindowOpt = Window::createWindow(800, 600, "Vulkan window");
    assert(mainWindowOpt.has_value());

    auto mainWindow = std::move(mainWindowOpt.value());
    while(!mainWindow.shouldClose())
    {
        mainWindow.pollEvents();
    }

    glfwTerminate();
    return 0;
}