#pragma once

struct ImFramerInterface
{
    virtual ~ImFramerInterface() = 0;

    virtual int initialize() = 0;
    virtual void next_frame() = 0;
    virtual void destroy() = 0;
};

inline ImFramerInterface::~ImFramerInterface() = default;