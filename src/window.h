#pragma once

#include <GLFW/glfw3.h>
#include <functional>
#include <memory>
#include <optional>

class Window
{
    friend std::optional<Window> createWindow(int, int, const char*);
    friend void glfwResizeCallback(GLFWwindow* glfwWindow, int width, int height);

  public:
    ~Window();
    Window(Window&&) = default;

    static std::optional<std::unique_ptr<Window>> createWindow(
        int width,
        int height,
        const char* name,
        std::function<void(uint32_t width, uint32_t height)> resizeCallback);

    void pollEvents();

    bool shouldClose();

    GLFWwindow* getWindowHandle();

  private:
    Window(GLFWwindow* window, std::function<void(uint32_t width, uint32_t height)>);

    std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> glfwWindow;
    std::function<void(uint32_t width, uint32_t height)> resizeCallback;

    static void glfwResizeCallback(GLFWwindow* glfwWindow, int width, int height);
};