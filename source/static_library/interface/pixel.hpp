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
    pixel<T> pix;
    pix.size = size;
    pix.memory.resize(size.x * size.y);
    pix.view = std::mdspan<T, std::dextents<int, 2>>(pix.memory.data(), size.y, size.x);
    return pix;
}
template <typename T> static inline pixel<T> make_pixel(glm::ivec2 size, std::span<T> source)
{
    pixel<T> pix;
    pix.size = size;
    pix.memory.resize(size.x * size.y);
    pix.view = std::mdspan<T, std::dextents<int, 2>>(pix.memory.data(), size.y, size.x);
    size_t copy_size = std::min(source.size(), pix.memory.size());
    std::copy_n(source.data(), copy_size, pix.memory.data());
    return pix;
}
