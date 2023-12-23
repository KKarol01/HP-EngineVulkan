#include <engine/window.hpp>
#include <engine/engine.hpp>

#include <fmt/core.h>
#include <GLFW/glfw3.h> 

namespace eng {

Window::Window(WindowSize size) noexcept {
    if(glfwInit() != GLFW_TRUE) {
        const char *error_message;
        glfwGetError(&error_message);
        return;
    }
    
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwwindow = glfwCreateWindow(size.width, size.height, "title", nullptr, nullptr);

    if(!glfwwindow) {
        const char *error_message;
        glfwGetError(&error_message);
        return;
    }

    glfwMakeContextCurrent(glfwwindow);
    glfwGetFramebufferSize(glfwwindow, (int*)&size_pixels.width, (int*)&size_pixels.height);
    glfwSetFramebufferSizeCallback(glfwwindow, [](auto window, auto width, auto height) {
        auto &w = Engine::get_window();
        assert(w.glfwwindow == window);
        w.size_pixels.width = width;
        w.size_pixels.height = height;
        w.resized = true; 
    });
    glfwSetDropCallback(glfwwindow, []([[maybe_unused]] auto window, auto count, auto paths) {
        if(count < 1) { return; }
        auto &w = Engine::get_window();
        assert(w.glfwwindow == window);
        w.payload = paths[0];
    });
}

Window::~Window() noexcept {
    glfwDestroyWindow(glfwwindow);
}

}