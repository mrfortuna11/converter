#pragma once
#include <cstdint>
#include <vector>

struct MipLevel {
    uint32_t             width;
    uint32_t             height;
    std::vector<uint8_t> pixels; // R8G8B8A8_UNORM
};

std::vector<MipLevel> generateMipsSlang(
    const uint8_t* srcPixels,
    uint32_t       width,
    uint32_t       height,
    const char*    shaderDir);
