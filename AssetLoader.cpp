#include "AssetLoader.hpp"
#include "read_write_chunk.hpp"
#include "data_path.hpp"
#include <fstream>
#include <iostream>

bool AssetLoader::load_assets(const std::string &filename, 
                             std::array<PPU466::Tile, 16 * 16> &tile_table,
                             std::array<PPU466::Palette, 8> &palette_table) {
    
    std::ifstream file(data_path(filename), std::ios::binary);
    if (!file) {
        std::cerr << "Failed to open asset file: " << filename << std::endl;
        return false;
    }
    
    try {
        // Load chunks with magic number verification
        std::vector<PPU466::Tile> tiles;
        std::vector<PPU466::Palette> palettes;
        
        read_chunk(file, "TILE", &tiles);      // Read and verify "TILE" magic
        read_chunk(file, "PALT", &palettes);   // Read and verify "PALT" magic
        
        // Copy to arrays (bounds checking)
        if (tiles.size() > tile_table.size()) {
            std::cerr << "Warning: Too many tiles, truncating to " << tile_table.size() << std::endl;
        }
        if (palettes.size() > palette_table.size()) {
            std::cerr << "Warning: Too many palettes, truncating to " << palette_table.size() << std::endl;
        }
        
        // Copy loaded data
        std::copy(tiles.begin(), 
                 tiles.begin() + std::min(tiles.size(), tile_table.size()), 
                 tile_table.begin());
        std::copy(palettes.begin(), 
                 palettes.begin() + std::min(palettes.size(), palette_table.size()), 
                 palette_table.begin());
        
        std::cout << "✓ Loaded " << filename << ": " << tiles.size() << " tiles, " << palettes.size() << " palettes" << std::endl;
        return true;
        
    } catch (std::exception const &e) {
        std::cerr << "Failed to read chunks from " << filename << ": " << e.what() << std::endl;
        return false;
    }
}