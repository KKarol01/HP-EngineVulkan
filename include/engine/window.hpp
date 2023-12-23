#pragma once 

#include <cstdint>
#include <string>

struct GLFWwindow;

namespace eng {

struct WindowSize {
    uint32_t width{0}, height{0};
};

class Window {
public:
    explicit Window(WindowSize size) noexcept;
    ~Window() noexcept;

    operator bool() const { return !!glfwwindow; }

    GLFWwindow* get_glfwptr() const { return glfwwindow; }
    void update() {
        resized = false;
        payload.clear();
    };
    bool file_dropped() const { return !payload.empty(); }
    bool properly_initialized() const { return !!glfwwindow; }

    WindowSize size_pixels{};
    bool resized{false};
    GLFWwindow *glfwwindow{nullptr};
    std::string payload;
};

}