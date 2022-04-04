#include "window.h"
#include <cassert>
#include <utility>

void glfwResizeCallback(GLFWwindow* glfwWindow, int width, int height)
{
    auto window = (Window*)glfwGetWindowUserPointer(glfwWindow);
    window->resizeCallback((uint32_t)width, (uint32_t)height);
}

std::optional<std::unique_ptr<Window>> Window::createWindow(
    int width,
    int height,
    const char* name,
    std::function<void(uint32_t width, uint32_t height)> resizeCallback)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* glfwWindow = glfwCreateWindow(width, height, name, nullptr, nullptr);
    assert(glfwWindow);

    auto window = std::make_unique<Window>(Window(glfwWindow, std::move(resizeCallback)));
    glfwSetWindowUserPointer(window->glfwWindow.get(), window.get());
    glfwSetFramebufferSizeCallback(window->glfwWindow.get(), glfwResizeCallback);

    if(glfwWindow)
        return window;
    else
        return std::nullopt;
}

Window::Window(
    GLFWwindow* window,
    std::function<void(uint32_t width, uint32_t height)> resizeCallback)
    : glfwWindow(window, glfwDestroyWindow)
    , resizeCallback(std::move(resizeCallback))
{
}

void Window::pollEvents()
{
    glfwPollEvents();
}

bool Window::shouldClose()
{
    return glfwWindowShouldClose(glfwWindow.get());
}

GLFWwindow* Window::getWindowHandle()
{
    return glfwWindow.get();
}