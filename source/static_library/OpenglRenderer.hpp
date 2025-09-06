#pragma once
#include <interface/RendererInterface.hpp>

#include <interface/CommandQueue.hpp>

#include <memory>
#include <string>

struct GLFWwindow;
class OpenglRenderer : public RendererInterface
{
public:
    int initialize() override;
    void render_loop(std::stop_token& token) override;
    void destroy() override;

public:
    void set_vertex_shader(const std::string& source);
    void set_fragment_shader(const std::string& source);
    void compile_shaders();

public:
    std::string vertex_shader_source;
    std::string fragment_shader_source;
    uint32_t user_shader_program = 0;
    uint32_t user_vertex_array_object = 0;
    std::shared_ptr<GLFWwindow> window;
    std::shared_ptr<CommandQueue> command_queue_on_begin;
    std::shared_ptr<CommandQueue> command_queue_on_swap_before;
};