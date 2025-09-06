#pragma once
#include "interface/ImFramerInterface.hpp"

class OpenglComputeShaderFramer : public ImFramerInterface
{
public:
    int initialize() override;
    void next_frame() override;
    void destroy() override;
};
