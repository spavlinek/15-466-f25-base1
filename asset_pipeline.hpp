#pragma once

#include "PPU466.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <map>

// Compare colors (needed for map)
struct ColorComparator {
    bool operator()(const glm::u8vec4 &a, const glm::u8vec4 &b) const {
        if (a.r != b.r) return a.r < b.r;
        if (a.g != b.g) return a.g < b.g;
        if (a.b != b.b) return a.b < b.b;
        return a.a < b.a;
    }
};

struct TileColorAnalysis {
    std::vector<glm::u8vec4> unique_colors;
    std::map<glm::u8vec4, uint8_t, ColorComparator> color_to_index;
    bool valid = false; // true if <= 4 colors
    uint8_t assigned_palette = 0; // Which palette this tile should use
};

// Main asset pipeline function
bool load_tileset_from_png(const std::string &png_filename, 
                          std::array<PPU466::Tile, 16 * 16> &tile_table,
                          std::array<PPU466::Palette, 8> &palette_table);

// Helper functions
TileColorAnalysis analyze_tile_colors(const std::vector<glm::u8vec4> &png_pixels,
                                     glm::uvec2 png_size,
                                     uint32_t tile_x, uint32_t tile_y);

uint8_t rgba_to_color_index(glm::u8vec4 pixel, const TileColorAnalysis &analysis);

void convert_png_to_tiles_and_palettes(const std::vector<glm::u8vec4> &png_pixels, 
                                       glm::uvec2 png_size,
                                       std::array<PPU466::Tile, 16 * 16> &tile_table,
                                       std::array<PPU466::Palette, 8> &palette_table);

//checks if palette already exists and if not creates it
uint8_t find_or_create_palette(const std::vector<glm::u8vec4> &colors,
                              std::array<PPU466::Palette, 8> &palette_table,
                              uint8_t &next_palette_slot);
