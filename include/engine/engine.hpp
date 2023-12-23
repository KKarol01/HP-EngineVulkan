#pragma once

#include <memory>

namespace eng {

class Renderer;
class Window;

class Engine {
public:
    Engine() = default;
    ~Engine();

    static void initialize();
    static void start();

    static size_t get_frame_number() { return _this->frame_number; }
    static Renderer& get_renderer() { return *_this->_renderer; }
    static Window& get_window() { return *_this->_window; }

private:
    size_t frame_number{0};
    inline static std::unique_ptr<Engine> _this;
    std::unique_ptr<Window> _window; 
    std::unique_ptr<Renderer> _renderer;
};

}