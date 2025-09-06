#include <array>
#include <atomic>
#include <future>
#include <iostream>

#include <fmt/color.h>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <fmt/std.h>

#include <Executor.hpp>
#include <ImRenderer.hpp>
#include <OpenglComputeShaderFramer.hpp>
#include <OpenglImRenderer.hpp>
#include <OpenglRasterizationFramer.hpp>
#include <OpenglRenderer.hpp>

int main(int, char**)
{
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto executor = std::make_shared<Executor>();
    auto im_renderer = std::make_shared<ImRenderer>();
    auto rasterization_framer = std::make_shared<OpenglRasterizationFramer>();
    im_renderer->set_framer(rasterization_framer);
    auto compute_shader_framer = std::make_shared<OpenglComputeShaderFramer>();
    im_renderer->set_framer(compute_shader_framer);

    auto opengl_renderer = std::make_shared<OpenglRenderer>();
    auto opengl_im_renderer = std::make_shared<OpenglImRenderer>();

    // executor->async_execute(im_renderer);
    // executor->sync_wait_destruction();
    return executor->execute(im_renderer);
}