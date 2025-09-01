#include "Sprites.hpp"
#include "read_write_chunk.hpp"
#include "data_path.hpp"
#include <fstream>
#include <iostream>

void Sprite::draw(PPU466 &ppu, int32_t x, int32_t y, uint32_t start_sprite_slot) const {
    draw_frame(ppu, x, y, start_sprite_slot, 0);  // Default to frame 0
}

void Sprite::draw(PPU466 &ppu, int32_t x, int32_t y, uint32_t start_sprite_slot, bool flip_x) const {
    draw_frame(ppu, x, y, start_sprite_slot, 0, flip_x);  // Default to frame 0 with flip
}

void Sprite::draw_frame(PPU466 &ppu, int32_t x, int32_t y, uint32_t start_sprite_slot, uint32_t frame) const {
    // Make sure we don't exceed sprite limits
    if (start_sprite_slot + tiles.size() > ppu.sprites.size()) {
        std::cerr << "Warning: Not enough sprite slots for " << name << std::endl;
        return;
    }
    
    // Clamp frame to valid range
    uint32_t actual_frame = (frame_count > 0) ? (frame % frame_count) : 0;
    
    // Draw each tile of the sprite
    for (size_t i = 0; i < tiles.size(); ++i) {
        const TileRef &tile_ref = tiles[i];
        PPU466::Sprite &hw_sprite = ppu.sprites[start_sprite_slot + i];
        
        // Calculate final position
        int32_t final_x = x + tile_ref.offset_x + origin_offset.x;
        int32_t final_y = y + tile_ref.offset_y + origin_offset.y;
        
        // Calculate animated tile index (frame offset)
        uint8_t animated_tile_index = tile_ref.tile_index + (actual_frame * tiles.size()/2);
        
        // Set sprite properties
        hw_sprite.x = uint8_t(std::max(0, std::min(255, final_x)));
        hw_sprite.y = uint8_t(std::max(0, std::min(255, final_y)));
        hw_sprite.index = animated_tile_index;
        hw_sprite.attributes = tile_ref.palette_index | tile_ref.attributes;
    }
}

void Sprite::draw_frame(PPU466 &ppu, int32_t x, int32_t y, uint32_t start_sprite_slot, uint32_t frame, bool flip_x) const {
    // Make sure we don't exceed sprite limits
    if (start_sprite_slot + tiles.size() > ppu.sprites.size()) {
        std::cerr << "Warning: Not enough sprite slots for " << name << std::endl;
        return;
    }
    
    // Clamp frame to valid range
    uint32_t actual_frame = (frame_count > 0) ? (frame % frame_count) : 0;
    
    if (!flip_x) {
        // Normal drawing (no flip)
        for (size_t i = 0; i < tiles.size(); ++i) {
            const TileRef &tile_ref = tiles[i];
            PPU466::Sprite &hw_sprite = ppu.sprites[start_sprite_slot + i];
            
            // Calculate final position
            int32_t final_x = x + tile_ref.offset_x + origin_offset.x;
            int32_t final_y = y + tile_ref.offset_y + origin_offset.y;
            
            // Calculate animated tile index (frame offset)
            uint8_t animated_tile_index = tile_ref.tile_index + (actual_frame * tiles.size()/2);
            
            // Set sprite properties
            hw_sprite.x = uint8_t(std::max(0, std::min(255, final_x)));
            hw_sprite.y = uint8_t(std::max(0, std::min(255, final_y)));
            hw_sprite.index = animated_tile_index;
            hw_sprite.attributes = tile_ref.palette_index | tile_ref.attributes;
        }
    } else {
        // Flipped drawing - we need to reserve temp tiles for flipped versions
        // For now, use the last tiles in the tile table as temporary storage
        uint8_t temp_tile_start = 252; // Use tiles 252-255 as temporary flip storage
        
        // Create flipped tiles and draw with repositioned coordinates
        for (size_t i = 0; i < tiles.size(); ++i) {
            const TileRef &tile_ref = tiles[i];
            PPU466::Sprite &hw_sprite = ppu.sprites[start_sprite_slot + i];
            
            // Calculate flipped position (mirror tiles within sprite bounds)
            int32_t final_x = x + (bounding_box.x - tile_ref.offset_x - 8) + origin_offset.x;
            int32_t final_y = y + tile_ref.offset_y + origin_offset.y;
            
            // Calculate animated tile index (frame offset)
            uint8_t original_tile_index = tile_ref.tile_index + (actual_frame * tiles.size()/2);
            uint8_t flipped_tile_index = temp_tile_start + i;
            
            // Copy and flip the tile data
            ppu.tile_table[flipped_tile_index] = ppu.tile_table[original_tile_index];
            
            // Flip each row of the tile horizontally by reversing bit order
            PPU466::Tile &tile = ppu.tile_table[flipped_tile_index];
            for (int row = 0; row < 8; ++row) {
                uint8_t original_bit0 = tile.bit0[row];
                uint8_t original_bit1 = tile.bit1[row];
                
                // Reverse the bits in each byte
                uint8_t flipped_bit0 = 0;
                uint8_t flipped_bit1 = 0;
                
                for (int bit = 0; bit < 8; ++bit) {
                    if (original_bit0 & (1 << bit)) {
                        flipped_bit0 |= (1 << (7 - bit));
                    }
                    if (original_bit1 & (1 << bit)) {
                        flipped_bit1 |= (1 << (7 - bit));
                    }
                }
                
                tile.bit0[row] = flipped_bit0;
                tile.bit1[row] = flipped_bit1;
            }
            
            // Set sprite properties with flipped tile
            hw_sprite.x = uint8_t(std::max(0, std::min(255, final_x)));
            hw_sprite.y = uint8_t(std::max(0, std::min(255, final_y)));
            hw_sprite.index = flipped_tile_index;
            hw_sprite.attributes = tile_ref.palette_index | tile_ref.attributes;
        }
    }
}

const Sprite* Sprites::lookup(const std::string &name) const {
    auto it = sprites.find(name);
    return (it != sprites.end()) ? &it->second : nullptr;
}

Sprite* Sprites::lookup(const std::string &name) {
    auto it = sprites.find(name);
    return (it != sprites.end()) ? &it->second : nullptr;
}

void Sprites::add_sprite(const std::string &name, const Sprite &sprite) {
    sprites[name] = sprite;
}

void Sprites::create_simple_sprite(const std::string &name, uint8_t tile_index, uint8_t palette_index) {
    Sprite sprite;
    sprite.name = name;
    sprite.tiles.push_back({tile_index, palette_index, 0, 0, 0});
    sprite.bounding_box = {8, 8};
    sprites[name] = sprite;
}

void Sprites::create_multi_tile_sprite(const std::string &name, 
                                      const std::vector<uint8_t> &tile_indices,
                                      uint8_t palette_index,
                                      glm::ivec2 grid_size) {
    Sprite sprite;
    sprite.name = name;
    sprite.bounding_box = {grid_size.x * 8, grid_size.y * 8};
    
    // Arrange tiles in a grid
    for (int y = 0; y < grid_size.y; ++y) {
        for (int x = 0; x < grid_size.x; ++x) {
            uint32_t index = x + y * grid_size.x;
            if (index < tile_indices.size()) {
                Sprite::TileRef tile_ref;
                tile_ref.tile_index = tile_indices[index];
                tile_ref.palette_index = palette_index;
                tile_ref.offset_x = x * 8;
                tile_ref.offset_y = y * 8;
                sprite.tiles.push_back(tile_ref);
            }
        }
    }
    
    sprites[name] = sprite;
}

void SpriteAnimator::update(float elapsed) {
    if (!playing || !sprite || sprite->frame_count <= 1) return;
    
    current_time += elapsed;
    if (current_time >= frame_time) {
        current_time -= frame_time;
        
        if (current_frame + 1 >= sprite->frame_count) {
            if (loop) {
                current_frame = 0;
            }
        } else {
            current_frame++;
        }
    }
}

Sprites Sprites::load(const std::string &filename) {
    Sprites result;
    
    std::ifstream file(data_path(filename), std::ios::binary);
    if (!file) {
        throw std::runtime_error("Failed to open sprite file: " + filename);
    }
    
    try {
        // Load sprite definitions from chunks
        std::vector<Sprite> sprite_list;
        read_chunk(file, "SPRT", &sprite_list);
        
        // Convert vector to map
        for (const auto &sprite : sprite_list) {
            result.sprites[sprite.name] = sprite;
        }
        
    } catch (std::exception const &e) {
        throw std::runtime_error("Failed to load sprites: " + std::string(e.what()));
    }
    
    return result;
}