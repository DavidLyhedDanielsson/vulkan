#pragma once

#include <GLFW/glfw3.h>
#include <memory>
#include <optional>


class Window
{
    friend std::optional<Window> createWindow(int, int, const char*);

  public:
    ~Window();
    Window(Window&&) = default;

    static std::optional<Window> createWindow(int width, int height, const char* name);

    void createWindow();
    void pollEvents();

    bool shouldClose();

    GLFWwindow* getWindowHandle();

  private:
    Window(GLFWwindow* window);

    std::unique_ptr<GLFWwindow, decltype(&glfwDestroyWindow)> window;
};