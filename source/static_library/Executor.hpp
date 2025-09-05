#pragma once
#include <interface/ExecutorInterface.hpp>

#include <atomic>
#include <latch>
#include <stop_token>
#include <thread>

class Executor : public ExecutorInterface
{
public:
    int execute(std::shared_ptr<RendererInterface> renderer) override;
    void async_execute(std::shared_ptr<RendererInterface> renderer) override;

    void sync_wait_initialization() override;
    void sync_wait_destruction() override;

private:
    std::latch initialization_latch{ 1 };
    std::latch destruction_latch{ 1 };
    std::jthread worker_thread;
    std::stop_source stop_source;
};