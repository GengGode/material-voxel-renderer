#pragma once
#include <mdspan>
#include <vector>

#include <glm/glm.hpp>

template <typename T> struct pixel
{
    glm::ivec2 size;
    std::vector<T> memory;
    std::mdspan<T, std::dextents<int, 2>> view; // layout: y, x

    T& operator()(int x, int y) { return view[y, x]; }
};

template <typename T> static inline pixel<T> make_pixel(glm::ivec2 size)
{
    pixel<T> vox;
    vox.size = size;
    vox.memory.resize(size.x * size.y);
    vox.view = std::mdspan<T, std::dextents<int, 2>>(vox.memory.data(), size.y, size.x);
    return vox;
}
