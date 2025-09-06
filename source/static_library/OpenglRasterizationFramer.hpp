#pragma once
#include "interface/ImFramerInterface.hpp"

class OpenglRasterizationFramer : public ImFramerInterface
{
public:
    int initialize() override;
    void next_frame() override;
    void destroy() override;
};
