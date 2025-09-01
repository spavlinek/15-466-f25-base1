#include "asset_pipeline.hpp"
#include "read_write_chunk.hpp"
#include "PPU466.hpp"
#include <iostream>
#include <fstream>

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.png> <output.dat>" << std::endl;
        return 1;
    }
    
    std::string input_png = argv[1];
    std::string output_file = argv[2];
    
    std::array<PPU466::Tile, 16 * 16> tile_table;
    std::array<PPU466::Palette, 8> palette_table;
    
    // Process PNG at BUILD TIME
    if (!load_tileset_from_png(input_png, tile_table, palette_table)) {
        std::cerr << "Failed to process PNG: " << input_png << std::endl;
        return 1;
    }
    
    // Convert arrays to vectors for write_chunk
    std::vector<PPU466::Tile> tiles(tile_table.begin(), tile_table.end());
    std::vector<PPU466::Palette> palettes(palette_table.begin(), palette_table.end());
    
    // Save using the chunk format
    std::ofstream out(output_file, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to create output file: " << output_file << std::endl;
        return 1;
    }
    
    try {
        write_chunk("TILE", tiles, &out);      // Magic: "TILE"
        write_chunk("PALT", palettes, &out);   // Magic: "PALT" 
        
        std::cout << "✓ Generated " << output_file << std::endl;
        std::cout << "  - " << tiles.size() << " tiles (" << tiles.size() * sizeof(PPU466::Tile) << " bytes)" << std::endl;
        std::cout << "  - " << palettes.size() << " palettes (" << palettes.size() * sizeof(PPU466::Palette) << " bytes)" << std::endl;
        
    } catch (std::exception const &e) {
        std::cerr << "Failed to write chunks: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}