#pragma once
#include <interface/RendererInterface.hpp>

#include <memory>

struct GLFWwindow;
struct ImFramerInterface;
class ImRenderer : public RendererInterface
{
public:
    int initialize() override;
    void render_loop(std::stop_token& token) override;
    void destroy() override;

public:
    void set_framer(std::shared_ptr<ImFramerInterface> framer);

public:
    std::shared_ptr<ImFramerInterface> framer;
    std::shared_ptr<GLFWwindow> window;
};