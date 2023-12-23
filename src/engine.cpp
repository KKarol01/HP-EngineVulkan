#include <iostream>
#include <memory>

#include <engine/engine.hpp>
#include <engine/window.hpp>
#include <engine/renderer.hpp>

namespace eng {

Engine::~Engine() = default;
    
void Engine::initialize() {
    try {
        _this = std::make_unique<Engine>();

        _this->_window = std::make_unique<Window>(eng::WindowSize{1024, 768});
        if(!_this->_window->properly_initialized()) {
            throw std::runtime_error{"Window is not initialized"};
        }
        
        _this->_renderer = std::make_unique<Renderer>(&*_this->_window);
        if(!_this->_renderer->is_properly_initialized()) {
            _this->_renderer.reset();
            throw std::runtime_error{"Renderer is not initialized!"};
        }
    } catch(const std::runtime_error &error) {
        std::cout << "The engine could not initialize for the following reason: " << error.what() << '\n';
    }
}

void Engine::start() {
    while(!glfwWindowShouldClose(_this->_window->get_glfwptr())) {
        ++_this->frame_number;
        _this->_renderer->update();
        _this->_window->update();
        glfwPollEvents();
    }
}

}