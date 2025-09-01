#pragma once

#include "PPU466.hpp"
#include <string>

// Simple, fast runtime loading - just memcpy!
class AssetLoader {
public:
    static bool load_assets(const std::string &filename, 
                           std::array<PPU466::Tile, 16 * 16> &tile_table,
                           std::array<PPU466::Palette, 8> &palette_table);
};