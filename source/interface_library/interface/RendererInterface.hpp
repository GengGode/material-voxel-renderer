#pragma once
#include <stop_token>

struct RendererInterface
{
    // Pure virtual destructor must have a definition, but not inline with the pure-specifier.
    virtual ~RendererInterface() = 0;

    virtual int initialize() = 0;
    virtual void render_loop(std::stop_token& token) = 0;
    virtual void destroy() = 0;
};

// Provide the required definition for the pure virtual destructor.
inline RendererInterface::~RendererInterface() = default;