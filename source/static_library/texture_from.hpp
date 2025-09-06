#pragma once
#include <global-register-error.hpp>

#include <glad/glad.h>

#include "interface/pixel.hpp"
#include "interface/voxel.hpp"

template <typename T> GLuint texture_from(const voxel<T>& vol, GLuint existed_tex3d = 0)
{
    GLuint tex3d = existed_tex3d;
    if (tex3d == 0)
        glGenTextures(1, &tex3d);

    glBindTexture(GL_TEXTURE_3D, tex3d);

    if constexpr (std::is_same_v<T, uint16_t>)
        glTexImage3D(GL_TEXTURE_3D, 0, GL_R16UI, vol.size.x, vol.size.y, vol.size.z, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, vol.memory.data());
    else if constexpr (std::is_same_v<T, uint8_t>)
        glTexImage3D(GL_TEXTURE_3D, 0, GL_R8UI, vol.size.x, vol.size.y, vol.size.z, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, vol.memory.data());
    else
        return code_err("{}: Unsupported voxel data type", __func__), 0;

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_3D, 0);
    return tex3d;
}

template <typename T> GLuint texture_from(const pixel<T>& img, GLuint existed_tex2d = 0)
{
    GLuint tex2d = existed_tex2d;
    if (tex2d == 0)
        glGenTextures(1, &tex2d);

    glBindTexture(GL_TEXTURE_2D, tex2d);

    if constexpr (std::is_same_v<T, uint32_t>)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img.size.x, img.size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.memory.data());
    else if constexpr (std::is_same_v<T, uint16_t>)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R16UI, img.size.x, img.size.y, 0, GL_RED_INTEGER, GL_UNSIGNED_SHORT, img.memory.data());
    else if constexpr (std::is_same_v<T, uint8_t>)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8UI, img.size.x, img.size.y, 0, GL_RED_INTEGER, GL_UNSIGNED_BYTE, img.memory.data());
    else
        return code_err("{}: Unsupported pixel data type", __func__), 0;

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glBindTexture(GL_TEXTURE_2D, 0);
    return tex2d;
}