#pragma once
#include <memory>

struct RendererInterface;
struct ExecutorInterface
{
    // Pure virtual destructor must have a definition, but not inline with the pure-specifier.
    virtual ~ExecutorInterface() = 0;

    /// @brief Initialize and render frame and destroy the renderer in the current thread.
    virtual int execute(std::shared_ptr<RendererInterface> renderer) = 0;
    /// @brief Initialize and render frame and destroy the renderer in a separate thread.
    virtual void async_execute(std::shared_ptr<RendererInterface> renderer) = 0;

    /// @brief Wait for the initialization to complete in async mode.
    virtual void sync_wait_initialization() = 0;
    /// @brief Wait for the destruction to complete in async mode.
    virtual void sync_wait_destruction() = 0;
};

// Provide the required definition for the pure virtual destructor.
inline ExecutorInterface::~ExecutorInterface() = default;