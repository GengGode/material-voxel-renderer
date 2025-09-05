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
#include <OpenglFramer.hpp>
#include <OpenglRenderer.hpp>

int main(int, char**)
{
    std::this_thread::sleep_for(std::chrono::seconds(1));

    auto executor = std::make_shared<Executor>();
    // auto im_renderer = std::make_shared<ImRenderer>();
    // auto framer = std::make_shared<OpenglFramer>();
    // im_renderer->set_framer(framer);

    auto opengl_renderer = std::make_shared<OpenglRenderer>();

    // executor->async_execute(im_renderer);
    // executor->sync_wait_destruction();
    return executor->execute(opengl_renderer);
}