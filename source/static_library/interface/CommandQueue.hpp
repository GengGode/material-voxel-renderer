#pragma once
#include <functional>

struct CommandQueue
{
    virtual ~CommandQueue() = 0;
    virtual void enqueue(std::function<void()> task) = 0;
    virtual void consume() = 0;
};

inline CommandQueue::~CommandQueue() = default;