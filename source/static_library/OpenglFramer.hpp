#pragma once
#include "interface/ImFramerInterface.hpp"

class OpenglFramer : public ImFramerInterface
{
public:
    int initialize() override;
    void next_frame() override;
    void destroy() override;
};
