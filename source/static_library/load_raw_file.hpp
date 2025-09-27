#pragma once

#include <filesystem>
#include <fstream>
#include <vector>

static inline std::vector<uint8_t> load_raw_file(const std::filesystem::path file)
{
    std::ifstream f(file, std::ios::binary);
    if (not f.is_open())
        return {};
    f.seekg(0, std::ios::end);
    size_t size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(size);
    f.read(reinterpret_cast<char*>(buffer.data()), size);
    return buffer;
}
