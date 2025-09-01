#pragma once

#include "PPU466.hpp"
#include <string>
#include <vector>
#include <map>
#include <glm/glm.hpp>

// Forward declaration
struct Sprites;

struct Sprite {
    struct TileRef {
        uint8_t tile_index = 0;      // Index into PPU466 tile table (0-255)
        uint8_t palette_index = 0;   // Index into PPU466 palette table (0-7)
        int8_t offset_x = 0;         // X offset from sprite origin (pixels)
        int8_t offset_y = 0;         // Y offset from sprite origin (pixels)
        uint8_t attributes = 0;      // PPU466 sprite attributes (priority, etc.)
    };
    static_assert(sizeof(TileRef) == 5, "TileRef should be compact");

    std::string name;                    // Sprite name for debugging
    std::vector<TileRef> tiles;          // List of tiles that make up this sprite
    glm::ivec2 origin_offset = {0, 0};   // Offset from sprite position to visual center
    glm::ivec2 bounding_box = {8, 8};    // Sprite dimensions in pixels
    
    // Draw sprite at position using PPU466
    void draw(PPU466 &ppu, int32_t x, int32_t y, uint32_t start_sprite_slot = 0) const;
    void draw(PPU466 &ppu, int32_t x, int32_t y, uint32_t start_sprite_slot, bool flip_x) const;
    
    // Draw specific animation frame
    void draw_frame(PPU466 &ppu, int32_t x, int32_t y, uint32_t start_sprite_slot, uint32_t frame) const;
    void draw_frame(PPU466 &ppu, int32_t x, int32_t y, uint32_t start_sprite_slot, uint32_t frame, bool flip_x) const;
    
    // Get required number of hardware sprites
    uint32_t get_sprite_count() const { return tiles.size(); }
    
    // Animation support
    uint32_t frame_count = 1;            // Number of animation frames
};

struct Sprites {
    std::map<std::string, Sprite> sprites;  // Fixed: was missing Sprite type
    
    // Lookup sprite by name
    const Sprite* lookup(const std::string &name) const;
    Sprite* lookup(const std::string &name);
    
    // Add sprite programmatically
    void add_sprite(const std::string &name, const Sprite &sprite);
    
    // Load sprites from file (for asset pipeline)
    static Sprites load(const std::string &filename);
    
    // Save sprites to file (for asset pipeline)
    void save(const std::string &filename) const;
    
    // Create simple single-tile sprite
    void create_simple_sprite(const std::string &name, uint8_t tile_index, uint8_t palette_index);
    
    // Create multi-tile sprite (for larger sprites)
    void create_multi_tile_sprite(const std::string &name, 
                                 const std::vector<uint8_t> &tile_indices,
                                 uint8_t palette_index,
                                 glm::ivec2 grid_size);  // e.g., {2, 2} for 16x16 sprite
};

// Sprite animation helper
struct SpriteAnimator {
    const Sprite *sprite = nullptr;
    float frame_time = 0.1f;        // Time per frame in seconds
    float current_time = 0.0f;
    uint32_t current_frame = 0;     // Current animation frame (state belongs to animator)
    bool loop = true;
    bool playing = true;
    
    void update(float elapsed);
    void play() { playing = true; }
    void pause() { playing = false; }
    void reset() { current_time = 0.0f; current_frame = 0; }
    void set_sprite(const Sprite *new_sprite) { sprite = new_sprite; reset(); }
    
    // Get the current frame, ensuring it's within bounds
    uint32_t get_current_frame() const {
        if (!sprite || sprite->frame_count == 0) return 0;
        return current_frame % sprite->frame_count;
    }
};