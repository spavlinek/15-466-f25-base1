#include "PlayMode.hpp"
#include "AssetLoader.hpp"

//for the GL_ERRORS() macro:
#include "gl_errors.hpp"

//for glm::value_ptr() :
#include <glm/gtc/type_ptr.hpp>

#include <random>

PlayMode::PlayMode() {
	std::cout << "=== INITIALIZING GAME ===" << std::endl;
	
	// Load assets first
	if (AssetLoader::load_assets("game1_tileset.dat", ppu.tile_table, ppu.palette_table)) {
		std::cout << "✓ Assets loaded successfully!" << std::endl;
		
		// Create game sprites
		create_game_sprites();
		
	} else {
		std::cout << "Asset loading failed, using fallback" << std::endl;
		{ //use tiles 0-16 as some weird dot pattern thing:
			std::array< uint8_t, 8*8 > distance;
			for (uint32_t y = 0; y < 8; ++y) {
				for (uint32_t x = 0; x < 8; ++x) {
					float d = glm::length(glm::vec2((x + 0.5f) - 4.0f, (y + 0.5f) - 4.0f));
					d /= glm::length(glm::vec2(4.0f, 4.0f));
					distance[x+8*y] = uint8_t(std::max(0,std::min(255,int32_t( 255.0f * d ))));
				}
			}
			for (uint32_t index = 0; index < 16; ++index) {
				PPU466::Tile tile;
				uint8_t t = uint8_t((255 * index) / 16);
				for (uint32_t y = 0; y < 8; ++y) {
					uint8_t bit0 = 0;
					uint8_t bit1 = 0;
					for (uint32_t x = 0; x < 8; ++x) {
						uint8_t d = distance[x+8*y];
						if (d > t) {
							bit0 |= (1 << x);
						} else {
							bit1 |= (1 << x);
						}
					}
					tile.bit0[y] = bit0;
					tile.bit1[y] = bit1;
				}
				ppu.tile_table[index] = tile;
			}
		}

		//use sprite 32 as a "player":
		ppu.tile_table[32].bit0 = {
			0b01111110,
			0b11111111,
			0b11111111,
			0b11111111,
			0b11111111,
			0b11111111,
			0b11111111,
			0b01111110,
		};
		ppu.tile_table[32].bit1 = {
			0b00000000,
			0b00000000,
			0b00011000,
			0b00100100,
			0b00000000,
			0b00100100,
			0b00000000,
			0b00000000,
		};

		//makes the outside of tiles 0-16 solid:
		ppu.palette_table[0] = {
			glm::u8vec4(0x00, 0x00, 0x00, 0x00),
			glm::u8vec4(0x00, 0x00, 0x00, 0xff),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00),
			glm::u8vec4(0x00, 0x00, 0x00, 0xff),
		};

		//makes the center of tiles 0-16 solid:
		ppu.palette_table[1] = {
			glm::u8vec4(0x00, 0x00, 0x00, 0x00),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00),
			glm::u8vec4(0x00, 0x00, 0x00, 0xff),
			glm::u8vec4(0x00, 0x00, 0x00, 0xff),
		};

		//used for the player:
		ppu.palette_table[7] = {
			glm::u8vec4(0x00, 0x00, 0x00, 0x00),
			glm::u8vec4(0xff, 0xff, 0x00, 0xff),
			glm::u8vec4(0x00, 0x00, 0x00, 0xff),
			glm::u8vec4(0x00, 0x00, 0x00, 0xff),
		};

		//used for the misc other sprites:
		ppu.palette_table[6] = {
			glm::u8vec4(0x00, 0x00, 0x00, 0x00),
			glm::u8vec4(0x88, 0x88, 0xff, 0xff),
			glm::u8vec4(0x00, 0x00, 0x00, 0xff),
			glm::u8vec4(0x00, 0x00, 0x00, 0x00),
		};

	}
	
	// Initialize player
	player_at = glm::vec2(128.0f, 120.0f);  // Center of 256x240 screen
	player_animator.set_sprite(sprites.lookup("player"));
	player_animator.frame_time = 0.2f;  // 5 FPS animation
	
	// Spawn some enemies for testing - positioned in center area
	spawn_enemy(glm::vec2(128.0f, 180.0f));  // Center-top
	spawn_enemy(glm::vec2(128.0f, 60.0f));   // Center-bottom
	
	// Create level elements (shelves, hearts, pots)
	create_level_elements();
	
	std::cout << "=== GAME INITIALIZED ===" << std::endl;
}

PlayMode::~PlayMode() {
}

bool PlayMode::handle_event(SDL_Event const &evt, glm::uvec2 const &window_size) {

	if (evt.type == SDL_EVENT_KEY_DOWN) {
		if (evt.key.key == SDLK_LEFT) {
			left.downs += 1;
			left.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_RIGHT) {
			right.downs += 1;
			right.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_UP) {
			up.downs += 1;
			up.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_DOWN) {
			down.downs += 1;
			down.pressed = true;
			return true;
		}
	} else if (evt.type == SDL_EVENT_KEY_UP) {
		if (evt.key.key == SDLK_LEFT) {
			left.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_RIGHT) {
			right.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_UP) {
			up.pressed = false;
			return true;
		} else if (evt.key.key == SDLK_DOWN) {
			down.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	// Player movement
	constexpr float PlayerSpeed = 60.0f;
	glm::vec2 move_dir(0.0f);
	
	if (left.pressed) move_dir.x -= 1.0f;
	if (right.pressed) move_dir.x += 1.0f;
	if (down.pressed) move_dir.y -= 1.0f;
	if (up.pressed) move_dir.y += 1.0f;
	
	// Normalize diagonal movement
	if (glm::length(move_dir) > 0.0f) {
		move_dir = glm::normalize(move_dir);
		glm::vec2 new_position = player_at + move_dir * PlayerSpeed * elapsed;
		
		// Update facing direction based on horizontal movement
		if (move_dir.x > 0.0f) {
			player_facing_right = true;
		} else if (move_dir.x < 0.0f) {
			player_facing_right = false;
		}
		// Don't change facing for purely vertical movement
		
		// Check collision with wood shelves
		if (!check_collision(new_position, {16.0f, 16.0f})) {  // Player is 16x16
			player_at = new_position;
		}
	}
	
	// Keep bee animation always playing (flying in place)
	player_animator.play();
	
	// Keep player on screen
	player_at.x = std::max(4.0f, std::min(252.0f, player_at.x));
	player_at.y = std::max(4.0f, std::min(236.0f, player_at.y));
	
	// Update player animation
	player_animator.update(elapsed);
	
	// Update enemies
	for (auto &enemy : enemies) {
		if (!enemy.active) continue;
		
		// Keep enemies stationary in the center area
		// No movement AI - enemies stay where they were spawned
		enemy.velocity = glm::vec2(0.0f, 0.0f);
		
		// No position updates since velocity is zero
		// enemy.position += enemy.velocity * elapsed;
		
		// No need for edge bouncing since they don't move
		
		// Update enemy animation
		enemy.animator.update(elapsed);
	}
	
	// Reset button press counters
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	// Simple background
	ppu.background_color = glm::u8vec4(0x10, 0x20, 0x30, 0xff);
	
	// Create background tilemap
	create_level_background();
	
	// No background scrolling
	ppu.background_position.x = 0;
	ppu.background_position.y = 0;
	
	// Clear all hardware sprites first
	for (auto &sprite : ppu.sprites) {
		sprite.y = 240; // Move off-screen
	}
	next_sprite_slot = 0;  // Reset sprite allocation
	
	// Draw player with animation
	if (const Sprite *player_sprite = sprites.lookup("player")) {
		uint32_t slot = allocate_sprite_slots(player_sprite->get_sprite_count());
		player_sprite->draw_frame(ppu, int32_t(player_at.x), int32_t(player_at.y), slot, player_animator.get_current_frame(), player_facing_right);
	}
	
	// Draw enemies with animation
	for (const auto &enemy : enemies) {
		if (!enemy.active) continue;
		
		if (const Sprite *enemy_sprite = sprites.lookup("enemy")) {
			uint32_t slot = allocate_sprite_slots(enemy_sprite->get_sprite_count());
			enemy_sprite->draw_frame(ppu, int32_t(enemy.position.x), int32_t(enemy.position.y), slot, enemy.animator.get_current_frame());
		}
	}
	
	// Draw wood shelves
	if (const Sprite *wood_sprite = sprites.lookup("wood")) {
		for (const auto &wood : wood_shelves) {
			uint32_t slot = allocate_sprite_slots(wood_sprite->get_sprite_count());
			wood_sprite->draw(ppu, int32_t(wood.position.x), int32_t(wood.position.y), slot);
		}
	}
	
	// Draw collectibles
	for (const auto &collectible : collectibles) {
		if (collectible.collected) continue;
		
		if (const Sprite *item_sprite = sprites.lookup(collectible.type)) {
			uint32_t slot = allocate_sprite_slots(item_sprite->get_sprite_count());
			item_sprite->draw(ppu, int32_t(collectible.position.x), int32_t(collectible.position.y), slot);
		}
	}
	
	//--- actually draw ---
	ppu.draw(drawable_size);
}

void PlayMode::create_game_sprites() {
	std::cout << "Creating game sprites..." << std::endl;
	
	// player bee sprite  
	sprites.create_multi_tile_sprite("player", {0, 1, 16, 17}, 0, {2, 2});
	
	//enemy toxic bubble sprite
	sprites.create_simple_sprite("enemy", 32, 5);
	sprites.create_multi_tile_sprite("bigEnemy", {64, 65, 66, 67, 80, 81, 82, 83, 96, 97, 98, 99, 100, 112, 113, 114, 115}, 5, {4, 4});
	
	// Create collectible items
	sprites.create_simple_sprite("heart", 8, 0);    
	sprites.create_simple_sprite("wood", 24, 4);
	sprites.create_simple_sprite("pot", 56, 7);
	sprites.create_simple_sprite("flower", 40, 0);
	
	
	// Set up sprite properties
	if (Sprite *player = sprites.lookup("player")) {
		player->frame_count = 2;  // 2-frame flying animation
		player->bounding_box = {16, 16};
	}
	
	if (Sprite *enemy = sprites.lookup("enemy")) {
		enemy->frame_count = 1;   // 1-frame idle animation
		enemy->bounding_box = {8, 8};
	}
	
	std::cout << "Created " << sprites.sprites.size() << " sprite types" << std::endl;
}

void PlayMode::spawn_enemy(glm::vec2 position) {
	Enemy enemy;
	enemy.position = position;
	enemy.velocity = glm::vec2(0.0f, 0.0f);  // Stationary enemies
	enemy.animator.set_sprite(sprites.lookup("enemy"));
	enemy.animator.frame_time = 0.3f;
	enemies.push_back(enemy);
}

uint32_t PlayMode::allocate_sprite_slots(uint32_t count) {
	if (next_sprite_slot + count > ppu.sprites.size()) {
		std::cerr << "Warning: Out of sprite slots!" << std::endl;
		return 0;  // Return first slot as fallback
	}
	uint32_t slot = next_sprite_slot;
	next_sprite_slot += count;
	return slot;
}

void PlayMode::create_level_background() {
	// Define single 4x4 window pattern
	uint32_t window_pattern[4][4] = {
		{ 4,  5,  6,  7},  // Top row
		{20, 21, 22, 23},  // Second row
		{36, 37, 38, 39},  // Third row
		{52, 53, 54, 55}   // Bottom row
	};
	
	for (uint32_t y = 0; y < PPU466::BackgroundHeight; ++y) {
		for (uint32_t x = 0; x < PPU466::BackgroundWidth; ++x) {
			uint32_t bg_index = x + y * PPU466::BackgroundWidth;
			
			// Calculate position within the 4x4 window pattern
			uint32_t pattern_x = x % 4;
			uint32_t pattern_y = y % 4;
			
			// Use single window pattern throughout
			uint32_t tile_index = window_pattern[pattern_y][pattern_x];
			uint32_t palette_index = 1;  // Use palette 1 for all windows
			
			if (bg_index < ppu.background.size()) {
				ppu.background[bg_index] = tile_index | (palette_index << 8);
			}
		}
	}
}

void PlayMode::create_level_elements() {
	std::cout << "Creating level elements..." << std::endl;
	
	// Create wood shelves - horizontal rows that act as platforms
	// Shelf 1: Lower shelf on the left side
	for (int x = 32; x <= 96; x += 8) {  // 8 wood blocks wide
		wood_shelves.push_back({{float(x), 80.0f}});
	}
	
	// Shelf 2: Middle shelf on the right side
	for (int x = 160; x <= 224; x += 8) {  // 8 wood blocks wide
		wood_shelves.push_back({{float(x), 140.0f}});
	}
	
	// Shelf 3: Upper shelf in the center
	for (int x = 96; x <= 160; x += 8) {  // 8 wood blocks wide
		wood_shelves.push_back({{float(x), 200.0f}});
	}
	
	// Place pots on top of shelves
	collectibles.push_back({{64.0f, 88.0f}, "pot", false});    // On lower shelf
	collectibles.push_back({{192.0f, 148.0f}, "pot", false});  // On middle shelf
	collectibles.push_back({{128.0f, 208.0f}, "pot", false});  // On upper shelf
	
	// Place 3 hearts in upper right corner
	collectibles.push_back({{220.0f, 220.0f}, "heart", false});
	collectibles.push_back({{220.0f, 210.0f}, "heart", false});
	collectibles.push_back({{220.0f, 200.0f}, "heart", false});
	
	std::cout << "Created " << wood_shelves.size() << " wood blocks and " << collectibles.size() << " collectibles" << std::endl;
}

bool PlayMode::check_collision(glm::vec2 position, glm::vec2 size) {
	// Check collision with wood shelves
	for (const auto &wood : wood_shelves) {
		// Simple AABB collision detection
		if (position.x < wood.position.x + wood.size.x &&
			position.x + size.x > wood.position.x &&
			position.y < wood.position.y + wood.size.y &&
			position.y + size.y > wood.position.y) {
			return true;  // Collision detected
		}
	}
	return false;  // No collision
}

void PlayMode::flip_tile_horizontally(PPU466::Tile &tile) {
	// Flip each row of the tile horizontally by reversing bit order
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
}
