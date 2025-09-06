#pragma once
#include <interface/CommandQueue.hpp>

#include <atomic>

#include <concurrent_queue.h>

class OpenglCommandQueue : public CommandQueue
{
    using safe_queue_t = Concurrency::concurrent_queue<std::function<void()>>;

public:
    void enqueue(std::function<void()> task) override { tasks.load()->push(std::move(task)); }
    void consume() override
    {
        if (tasks.load()->empty())
            return;

        auto swaped = tasks.load();
        tasks.store(std::make_shared<safe_queue_t>());

        std::function<void()> task;
        while (swaped->try_pop(task) && task)
            task();
    }

private:
    std::atomic<std::shared_ptr<safe_queue_t>> tasks = std::make_shared<safe_queue_t>();
};
