#pragma once
#include "OpenglRenderer.hpp"

class OpenglImRenderer : public OpenglRenderer
{
public:
    int initialize() override;
    void destroy() override;
};