#include "Executor.hpp"
#include <interface/RendererInterface.hpp>

#define SPDLOG_NO_ATOMIC_LEVELS
#include <fmt/std.h>
#include <spdlog/sinks/msvc_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <global-register-error.hpp>

int Executor::execute(std::shared_ptr<RendererInterface> renderer)
{
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto msvc_sink = std::make_shared<spdlog::sinks::msvc_sink_mt>();
    spdlog::logger logger("executor", { console_sink, msvc_sink });
    spdlog::set_default_logger(std::make_shared<spdlog::logger>(logger));
    spdlog::set_level(spdlog::level::debug); // Set global log level to debug
    spdlog::flush_on(spdlog::level::debug);
    spdlog::set_pattern("[%H:%M:%S.%e] [th-%-6t] [%^%l%$] [%!] %v");

    SPDLOG_INFO("started");

    if (!renderer)
    {
        initialization_latch.count_down();
        destruction_latch.count_down();
        return code_err("{}: renderer is nullptr", __func__);
    }

    // Initialization code
    if (auto init_result = renderer->initialize(); init_result != 0)
    {
        initialization_latch.count_down();
        destruction_latch.count_down();
        return code_err("{}: renderer failed to initialize, init_result = {}", __func__, init_result);
    }
    initialization_latch.count_down();

    // Rendering loop
    auto token = stop_source.get_token();
    renderer->render_loop(token);

    // Destruction code
    renderer->destroy();
    destruction_latch.count_down();
    spdlog::shutdown();
    return 0;
}

void Executor::async_execute(std::shared_ptr<RendererInterface> renderer)
{
    worker_thread = std::jthread([this, renderer]() { this->execute(renderer); });
}

void Executor::sync_wait_initialization()
{
    initialization_latch.wait();
}

void Executor::sync_wait_destruction()
{
    destruction_latch.wait();
}