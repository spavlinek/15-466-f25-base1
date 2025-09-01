#include "asset_pipeline.hpp"
#include "load_save_png.hpp"
#include "data_path.hpp"
#include <iostream>
#include <set>
#include <map>

    //TODO:call load png and get the vector of colors
    // TODO: add error handling for file loading
    // TODO:validate PNG dimensions (should be multiple of 8)
    // TODO: handle case where PNG has more than 4 unique colors per 8x8 tile
    //TODO: isolate 8x8 blocks using math and make them into tiles and pallete
        //To convert into tile:
        //TODO: convert 4-color palette to 2-bit indices (0-3)
        //TODO: convert indices to bit planes (bit0, bit1 arrays)
        //TODO: map local palettes to global palette table
    //TODO: store the tiles and pallete for the ppu to use in ppu.tile_table and ppu.palette_table
    //save the binary .tiles file

// Compare colors (needed for map)


bool load_tileset_from_png(const std::string &png_filename, 
                          std::array<PPU466::Tile, 16 * 16> &tile_table,
                          std::array<PPU466::Palette, 8> &palette_table) {
    
    std::cout << "ASSET PIPELINE STARTING" << std::endl;
    
    //Load PNG and validate
    glm::uvec2 size;
    std::vector<glm::u8vec4> pixels;
    
    try {
        std::cout << "Loading PNG: " << png_filename << std::endl;
        load_png(data_path(png_filename), &size, &pixels, LowerLeftOrigin);
        std::cout << "PNG loaded: " << size.x << "x" << size.y << " pixels" << std::endl;
        
    } catch (std::exception const &e) {
        std::cerr << "Failed to load PNG: " << e.what() << std::endl;
        return false;
    }
    
    // make sure dimensions are correct aka multiple of 8
    if (size.x % 8 != 0 || size.y % 8 != 0) {
        std::cerr << "PNG dimensions must be multiple of 8! Got: " << size.x << "x" << size.y << std::endl;
        return false;
    }
    
    uint32_t tiles_x = size.x / 8;
    uint32_t tiles_y = size.y / 8;
    uint32_t total_tiles = tiles_x * tiles_y;
    
    std::cout << "Valid tile dimensions: " << tiles_x << "x" << tiles_y << " tiles (" << total_tiles << " total)" << std::endl;
    
    if (total_tiles > 256) {
        std::cerr << "Too many tiles! Max 256, found " << total_tiles << std::endl;
        return false;
    }
    
    // Step 3: Convert PNG to tiles
    std::cout << "Converting pixels to tiles and palettes..." << std::endl;
    convert_png_to_tiles_and_palettes(pixels, size, tile_table, palette_table);
    std::cout << "Conversion complete!" << std::endl;
    
    std::cout << "ASSET PIPELINE COMPLETE" << std::endl;
    return true;
}

uint8_t find_or_create_palette(const std::vector<glm::u8vec4> &colors,
                              std::array<PPU466::Palette, 8> &palette_table,
                              uint8_t &next_palette_slot) {
    
    // Check if we already have a palette with these exact colors
    for (uint8_t p = 0; p < next_palette_slot; ++p) {
        bool matches = true;
        for (size_t i = 0; i < 4; ++i) {
            if (i < colors.size()) {
                // Check if color matches (with some tolerance for rounding)
                auto existing = palette_table[p][i];
                auto needed = colors[i];
                if (abs(int(existing.r) - int(needed.r)) > 2 ||
                    abs(int(existing.g) - int(needed.g)) > 2 ||
                    abs(int(existing.b) - int(needed.b)) > 2 ||
                    abs(int(existing.a) - int(needed.a)) > 2) {
                    matches = false;
                    break;
                }
            }
        }
        if (matches) {
            std::cout << "    Reusing existing palette " << (int)p << std::endl;
            return p;
        }
    }
    
    // Create new palette
    if (next_palette_slot >= 8) {
        std::cout << "    Warning: Out of palette slots! Reusing palette 0" << std::endl;
        return 0;
    }
    
    std::cout << "    Creating new palette " << (int)next_palette_slot << std::endl;
    
    // Fill palette with the tile's colors
    for (size_t i = 0; i < 4; ++i) {
        if (i < colors.size()) {
            palette_table[next_palette_slot][i] = colors[i];
            std::cout << "      Color " << i << ": RGBA(" 
                     << (int)colors[i].r << "," << (int)colors[i].g << "," 
                     << (int)colors[i].b << "," << (int)colors[i].a << ")" << std::endl;
        } else {
            // Pad with transparent
            palette_table[next_palette_slot][i] = glm::u8vec4(0, 0, 0, 0);
        }
    }
    
    return next_palette_slot++;
}

void convert_png_to_tiles_and_palettes(const std::vector<glm::u8vec4> &png_pixels, 
                                       glm::uvec2 png_size,
                                       std::array<PPU466::Tile, 16 * 16> &tile_table,
                                       std::array<PPU466::Palette, 8> &palette_table) {
    
    uint32_t tiles_x = png_size.x / 8;
    uint32_t tiles_y = png_size.y / 8;
    uint8_t next_palette_slot = 0;
    
    // Clear all tiles first
    for (auto &tile : tile_table) {
        tile.bit0.fill(0);
        tile.bit1.fill(0);
    }
    
    // Clear all palettes
    for (auto &palette : palette_table) {
        palette.fill(glm::u8vec4(0, 0, 0, 0));
    }
    
    std::cout << "Processing " << tiles_x << " x " << tiles_y << " tiles..." << std::endl;
    
    // Process each 8x8 tile
    for (uint32_t tile_y = 0; tile_y < tiles_y && tile_y < 16; ++tile_y) {
        for (uint32_t tile_x = 0; tile_x < tiles_x && tile_x < 16; ++tile_x) {
            uint32_t tile_index = tile_x + tile_y * 16;
            PPU466::Tile &tile = tile_table[tile_index];
            
            // Analyze colors in this tile
            TileColorAnalysis analysis = analyze_tile_colors(png_pixels, png_size, tile_x, tile_y);
            
            std::cout << "Tile " << tile_index << ": " << analysis.unique_colors.size() << " colors";
            
            if (!analysis.valid) {
                std::cout << " (TOO MANY COLORS! Skipping)" << std::endl;
                continue;
            }
            
            // Find or create palette for this tile
            analysis.assigned_palette = find_or_create_palette(analysis.unique_colors, palette_table, next_palette_slot);
            std::cout << " -> palette " << (int)analysis.assigned_palette << std::endl;
            
            // Convert each pixel in this tile
            for (uint32_t y = 0; y < 8; ++y) {
                uint8_t bit0_row = 0;
                uint8_t bit1_row = 0;
                
                for (uint32_t x = 0; x < 8; ++x) {
                    uint32_t px = tile_x * 8 + x;
                    uint32_t py = tile_y * 8 + y;
                    uint32_t pixel_index = px + py * png_size.x;
                    
                    if (pixel_index < png_pixels.size()) {
                        glm::u8vec4 pixel = png_pixels[pixel_index];
                        uint8_t color_index = rgba_to_color_index(pixel, analysis);
                        
                        // Convert color index to bit planes
                        if (color_index & 1) bit0_row |= (1 << x);
                        if (color_index & 2) bit1_row |= (1 << x);
                    }
                }
                
                // With LowerLeftOrigin, PNG row 0 is already bottom row
                // PPU466 expects bit0[0] = bottom row, so direct mapping works
                tile.bit0[y] = bit0_row;
                tile.bit1[y] = bit1_row;
            }
        }
    }
    
    std::cout << "Generated " << (int)next_palette_slot << " unique palettes" << std::endl;
}

TileColorAnalysis analyze_tile_colors(const std::vector<glm::u8vec4> &png_pixels,
                                     glm::uvec2 png_size,
                                     uint32_t tile_x, uint32_t tile_y) {
    TileColorAnalysis result;
    std::set<glm::u8vec4, ColorComparator> unique_colors_set;
    
    // Collect all unique colors in this 8x8 tile
    for (uint32_t y = 0; y < 8; ++y) {
        for (uint32_t x = 0; x < 8; ++x) {
            uint32_t px = tile_x * 8 + x;
            uint32_t py = tile_y * 8 + y;
            uint32_t pixel_index = px + py * png_size.x;
            
            if (pixel_index < png_pixels.size()) {
                glm::u8vec4 pixel = png_pixels[pixel_index];
                
                // Treat transparent pixels as one color
                if (pixel.a < 128) {
                    pixel = glm::u8vec4(0, 0, 0, 0); // Normalize transparent
                }
                
                unique_colors_set.insert(pixel);
            }
        }
    }
    
    // Convert set to vector
    result.unique_colors.assign(unique_colors_set.begin(), unique_colors_set.end());
    
    // Check if we have <= 4 colors
    if (result.unique_colors.size() <= 4) {
        result.valid = true;
        
        // Create mapping from color to index
        for (size_t i = 0; i < result.unique_colors.size(); ++i) {
            result.color_to_index[result.unique_colors[i]] = uint8_t(i);
        }
        
        // Pad with transparent if we have < 4 colors
        while (result.unique_colors.size() < 4) {
            result.unique_colors.push_back(glm::u8vec4(0, 0, 0, 0));
        }
    }
    
    return result;
}

uint8_t rgba_to_color_index(glm::u8vec4 pixel, const TileColorAnalysis &analysis) {
    if (!analysis.valid) {
        std::cerr << "Color analysis not valid" << std::endl;
        return 0;
    }
    
    // Normalize transparent pixels
    if (pixel.a < 128) {
        pixel = glm::u8vec4(0, 0, 0, 0);
    }
    
    auto it = analysis.color_to_index.find(pixel);
    if (it != analysis.color_to_index.end()) {
        return it->second;
    }
    
    return 0; // Default to transparent
}