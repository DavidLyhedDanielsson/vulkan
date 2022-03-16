#include "window.h"
#include <assert.h>

std::optional<Window> Window::createWindow(int width, int height, const char* name)
{
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* glfwWindow = glfwCreateWindow(width, height, name, nullptr, nullptr);
    if(glfwWindow)
        return Window(glfwWindow);
    else
        return std::nullopt;
}

Window::Window(GLFWwindow* window): window(window, glfwDestroyWindow) {}

Window::~Window() {}

void Window::createWindow() {}

void Window::pollEvents()
{
    assert(window.get());

    glfwPollEvents();
}

bool Window::shouldClose()
{
    assert(window.get());

    return glfwWindowShouldClose(window.get());
}
