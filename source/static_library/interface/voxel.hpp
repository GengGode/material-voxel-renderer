#pragma once
#include <mdspan>
#include <vector>

#include <glm/glm.hpp>

template <typename T> struct voxel
{
    glm::ivec3 size;
    std::vector<T> memory;
    std::mdspan<T, std::dextents<int, 3>> view; // layout: z, y, x

    T& operator()(int x, int y, int z) { return view[z, y, x]; }
};

template <typename T> static inline voxel<T> make_voxel(glm::ivec3 size)
{
    voxel<T> vox;
    vox.size = size;
    vox.memory.resize(size.x * size.y * size.z);
    vox.view = std::mdspan<T, std::dextents<int, 3>>(vox.memory.data(), size.z, size.y, size.x);
    return vox;
}

#include <cereal/cereal.hpp>
template <typename Archive, typename T> void serialize(Archive& ar, voxel<T>& vox)
{
    ar(typeid(T).hash_code());
    ar(vox.size);
    if (Archive::is_loading::value)
    {
        vox.memory.resize(vox.size.x * vox.size.y * vox.size.z);
        vox.view = std::mdspan<T, std::dextents<int, 3>>(vox.memory.data(), vox.size.z, vox.size.y, vox.size.x);
    }
    ar(cereal::binary_data(vox.memory.data(), sizeof(T) * vox.memory.size()));
}
template <typename Archive, typename T> void load_and_construct(Archive& ar, voxel<T>& vox, const unsigned int /*version*/)
{
    ar(typeid(T).hash_code());
    ar(vox.size);
    vox.memory.resize(vox.size.x * vox.size.y * vox.size.z);
    vox.view = std::mdspan<T, std::dextents<int, 3>>(vox.memory.data(), vox.size.z, vox.size.y, vox.size.x);
    ar(cereal::binary_data(vox.memory.data(), sizeof(T) * vox.memory.size()));
}