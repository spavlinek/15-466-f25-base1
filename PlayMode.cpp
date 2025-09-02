#include "PlayMode.hpp"
#include "AssetLoader.hpp"

//for the GL_ERRORS() macro:
#include "gl_errors.hpp"

//for glm::value_ptr() :
#include <glm/gtc/type_ptr.hpp>

#include <random>

PlayMode::PlayMode() {
	
	// Load assets first
	if (AssetLoader::load_assets("game1_tileset.dat", ppu.tile_table, ppu.palette_table)) {
		
		// Create game sprites
		create_game_sprites();
		
	} else {
		std::cout << "Asset loading failed." << std::endl;
	
	}
	
	// Initialize player
	player_at = glm::vec2(0.0f, 0.0f);  // lower left corner of the screen
	player_animator.set_sprite(sprites.lookup("player"));
	player_animator.frame_time = 0.2f;  // 5 FPS animation
	
	// Create level elements using ASCII map
	load_level_from_map(get_level_map());
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
		} else if (evt.key.key == SDLK_A) {
			action.downs += 1;
			action.pressed = true;
			return true;
		} else if (evt.key.key == SDLK_R) {
			// Restart game when 'R' is pressed during game over
			if (game_over) {
				restart_game();
				return true;
			}
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
		} else if (evt.key.key == SDLK_A) {
			action.pressed = false;
			return true;
		}
	}

	return false;
}

void PlayMode::update(float elapsed) {
	// Don't update game logic during game over
	if (game_over) {
		return;
	}
	
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
		
		// Update enemy movement if they have a path
		if (enemy.path_distance > 0.0f) {
			float move_speed = 30.0f; // pixels per second
			float distance_this_frame = move_speed * elapsed;
			
			if (enemy.moving_forward) {
				enemy.current_distance += distance_this_frame;
				enemy.position += enemy.move_direction * distance_this_frame;
				
				// Check if reached end of path
				if (enemy.current_distance >= enemy.path_distance) {
					enemy.moving_forward = false;
					enemy.current_distance = enemy.path_distance;
					enemy.position = enemy.start_position + enemy.move_direction * enemy.path_distance;
				}
			} else {
				enemy.current_distance -= distance_this_frame;
				enemy.position -= enemy.move_direction * distance_this_frame;
				
				// Check if reached start of path
				if (enemy.current_distance <= 0.0f) {
					enemy.moving_forward = true;
					enemy.current_distance = 0.0f;
					enemy.position = enemy.start_position;
				}
			}
		}
		
		// Update enemy animation
		enemy.animator.update(elapsed);
	}
	
	// Update invulnerability timer
	if (invulnerability_timer > 0.0f) {
		invulnerability_timer -= elapsed;
	}
	
	// Check for enemy collisions
	check_enemy_collisions();
	
	// Check for pot interaction when 'A' is pressed
	if (action.downs > 0) {
		check_pot_interaction();
	}
	
	// Reset button press counters
	left.downs = 0;
	right.downs = 0;
	up.downs = 0;
	down.downs = 0;
	action.downs = 0;
}

void PlayMode::draw(glm::uvec2 const &drawable_size) {
	ppu.background_color = glm::u8vec4(0x10, 0x20, 0x30, 0xff);
	
	// Create background tilemap
	create_level_background();
	
	// Add wood tiles on top of background pattern
	update_background_with_wood();
	
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
	
	// Draw collectibles
	for (const auto &collectible : collectibles) {
		if (collectible.collected) continue;
		
		if (const Sprite *item_sprite = sprites.lookup(collectible.type)) {
			uint32_t slot = allocate_sprite_slots(item_sprite->get_sprite_count());
			item_sprite->draw(ppu, int32_t(collectible.position.x), int32_t(collectible.position.y), slot);
		}
	}
	
	// Draw health display in top-right corner
	if (const Sprite *heart_sprite = sprites.lookup("heart")) {
		for (int i = 0; i < player_health; ++i) {
			uint32_t slot = allocate_sprite_slots(heart_sprite->get_sprite_count());
			int32_t heart_x = 248 - (i * 12) - 8; // Start from right side and move left
			int32_t heart_y = 232;
			heart_sprite->draw(ppu, heart_x, heart_y, slot);
		}
	}
	
	// Draw game over screen
	if (game_over) {
		draw_game_over_screen();
	}
	
	//--- actually draw ---
	ppu.draw(drawable_size);
}

void PlayMode::create_game_sprites() {
	
	// player bee sprite  
	sprites.create_multi_tile_sprite("player", {0, 1, 16, 17}, 0, {2, 2});
	
	//enemy toxic bubble sprite
	sprites.create_simple_sprite("enemy", 32, 5);
	
	//items thingies
	sprites.create_simple_sprite("heart", 8, 0);    
	sprites.create_simple_sprite("wood", 24, 4);
	sprites.create_simple_sprite("pot", 56, 6);
	sprites.create_simple_sprite("flower", 40, 0);
	
	
	// Set up sprite properties
	if (Sprite *player = sprites.lookup("player")) {
		player->frame_count = 2;  // 2-frame bee flying animation
		player->bounding_box = {16, 16};
	}
	
	if (Sprite *enemy = sprites.lookup("enemy")) {
		enemy->frame_count = 1;
		enemy->bounding_box = {8, 8};
	}
	
}

void PlayMode::spawn_enemy(glm::vec2 position) {
	Enemy enemy;
	enemy.position = position;
	enemy.start_position = position;
	enemy.velocity = glm::vec2(0.0f, 0.0f);  // Stationary enemies
	enemy.move_direction = glm::vec2(0.0f, 0.0f);
	enemy.path_distance = 0.0f;
	enemy.current_distance = 0.0f;
	enemy.moving_forward = true;
	enemy.animator.set_sprite(sprites.lookup("enemy"));
	enemy.animator.frame_time = 0.3f;
	enemies.push_back(enemy);
}

void PlayMode::spawn_enemy_with_direction(glm::vec2 position, glm::vec2 direction, float distance) {
	Enemy enemy;
	enemy.position = position;
	enemy.start_position = position;
	enemy.velocity = glm::normalize(direction) * 30.0f;  // 30 pixels per second
	enemy.move_direction = glm::normalize(direction);
	enemy.path_distance = distance;
	enemy.current_distance = 0.0f;
	enemy.moving_forward = true;
	enemy.animator.set_sprite(sprites.lookup("enemy"));
	enemy.animator.frame_time = 0.3f;
	enemies.push_back(enemy);
}

uint32_t PlayMode::allocate_sprite_slots(uint32_t count) {
	if (next_sprite_slot + count > ppu.sprites.size()) {
		std::cerr << "Warning: Out of sprite slots!" << std::endl;
		return 0;
	}
	uint32_t slot = next_sprite_slot;
	next_sprite_slot += count;
	return slot;
}

void PlayMode::create_level_background() {
	uint32_t window_pattern[4][4] = {
		{ 4,  5,  6,  7},
		{20, 21, 22, 23},
		{36, 37, 38, 39},
		{52, 53, 54, 55}
	};
	
	for (uint32_t y = 0; y < PPU466::BackgroundHeight; ++y) {
		for (uint32_t x = 0; x < PPU466::BackgroundWidth; ++x) {
			uint32_t bg_index = x + y * PPU466::BackgroundWidth;
			
			uint32_t pattern_x = x % 4;
			uint32_t pattern_y = y % 4;
			
			uint32_t tile_index = window_pattern[pattern_y][pattern_x];
			uint32_t palette_index = 1;
			
			if (bg_index < ppu.background.size()) {
				ppu.background[bg_index] = tile_index | (palette_index << 8);
			}
		}
	}
}



void PlayMode::update_background_with_wood() {
	// Place wood tiles in the background
	for (const auto &wood_pos : wood_tile_positions) {
		if (wood_pos.x >= 0 && wood_pos.x < (int)PPU466::BackgroundWidth &&
		    wood_pos.y >= 0 && wood_pos.y < (int)PPU466::BackgroundHeight) {
			
			uint32_t bg_index = wood_pos.x + wood_pos.y * PPU466::BackgroundWidth;
			if (bg_index < ppu.background.size()) {
				// tile 24 (wood tile) with palette 4
				ppu.background[bg_index] = 24 | (4 << 8);
			}
		}
	}
}

bool PlayMode::check_collision(glm::vec2 position, glm::vec2 size) {
	int left_tile = int(position.x / 8.0f);
	int right_tile = int((position.x + size.x - 1) / 8.0f);
	int bottom_tile = int(position.y / 8.0f);
	int top_tile = int((position.y + size.y - 1) / 8.0f);
	
	// Check if any of the tiles the sprite overlaps contain wood
	for (int y = bottom_tile; y <= top_tile; ++y) {
		for (int x = left_tile; x <= right_tile; ++x) {
			// Check if this tile position contains wood
			for (const auto &wood_pos : wood_tile_positions) {
				if (wood_pos.x == x && wood_pos.y == y) {
					return true;  // Collision detected
				}
			}
		}
	}
	return false;
}

void PlayMode::load_level_from_map(const std::vector<std::string> &level_map) {
	wood_tile_positions.clear();
	enemies.clear();
	collectibles.clear();
	
	
	// Map coordinates: top-left is (0,0), but PPU466 uses bottom-left origin
	int map_height = level_map.size();
	
	for (int row = 0; row < map_height; ++row) {
		if (row >= (int)level_map.size()) break;
		
		const std::string &line = level_map[row];
		for (int col = 0; col < (int)line.length(); ++col) {
			char c = line[col];
			
			// Convert map coordinates to screen coordinates and tile coordinates
			float pixel_x = col * 8.0f;
			float pixel_y = (map_height - 1 - row) * 8.0f;
			int tile_x = col;
			int tile_y = map_height - 1 - row;
			
			switch (c) {
				case '#': // Wood block - now stored as background tile position
					wood_tile_positions.push_back({tile_x, tile_y});
					break;
				case 'E': // Enemy - stationary
					spawn_enemy(glm::vec2(pixel_x, pixel_y));
					break;
				case 'L': // Enemy - moves left/right
					spawn_enemy_with_direction(glm::vec2(pixel_x, pixel_y), glm::vec2(1.0f, 0.0f), 32.0f);
					break;
				case 'U': // Enemy - moves up/down  
					spawn_enemy_with_direction(glm::vec2(pixel_x, pixel_y), glm::vec2(0.0f, 1.0f), 24.0f);
					break;
				case 'H': // Heart
					collectibles.push_back({{pixel_x, pixel_y}, "heart", false});
					break;
				case 'P': // Pot
					collectibles.push_back({{pixel_x, pixel_y}, "pot", false});
					break;
				case 'F': // Flower
					collectibles.push_back({{pixel_x, pixel_y}, "flower", false});
					break;
				case '.': // Empty space
				case ' ': // Empty space
				default:
					break;
			}
		}
	}
	
	update_background_with_wood();
}

void PlayMode::check_enemy_collisions() {
	if (invulnerability_timer > 0.0f) return;
	
	const float collision_distance = 8.0f; // TODO: change if needed
	
	for (const auto &enemy : enemies) {
		if (!enemy.active) continue;
		
		// Calculate distance between player and enemy
		float distance = glm::length(player_at - enemy.position);
		
		if (distance <= collision_distance) {
			// Player hit by enemy - take damage
			player_health--;
			invulnerability_timer = 1.0f; // 1 second of invulnerability
			
			// Check for game over
			if (player_health <= 0) {
				std::cout << "Game Over! Press R to restart." << std::endl;
				player_health = 0; // Prevent negative health
				game_over = true;  // Enter game over state
			}
			
			break; // Only take damage from one enemy per frame
		}
	}
}

void PlayMode::check_pot_interaction() {
	// Check if player is near any pot
	const float interaction_distance = 16.0f;  // Player can interact within 16 pixels
	
	for (auto &collectible : collectibles) {
		if (collectible.type == "pot" && !collectible.collected) {
			// Calculate distance between player and pot
			float distance = glm::length(player_at - collectible.position);
			
			if (distance <= interaction_distance) {
				// Player is near this pot - spawn a flower above it
				glm::vec2 flower_position = collectible.position + glm::vec2(0.0f, 7.0f);  // 7 pixels above pot
				
				// Check if there's already a flower at this position
				bool flower_exists = false;
				for (const auto &item : collectibles) {
					if (item.type == "flower" && 
					    glm::length(item.position - flower_position) < 4.0f) {
						flower_exists = true;
						break;
					}
				}
				
				// Only spawn flower if one doesn't already exist here
				if (!flower_exists) {
					collectibles.push_back({flower_position, "flower", false});
				}
				
				// Only interact with the closest pot
				break;
			}
		}
	}
}

void PlayMode::draw_game_over_screen() {
	uint32_t screen_width_tiles = 32;
	uint32_t screen_height_tiles = 30; 
	uint32_t center_x = screen_width_tiles / 2; 
	uint32_t center_y = screen_height_tiles / 2;
	
	// Draw "GAME OVER" text - 2 rows of 9 tiles each
	uint32_t game_over_tiles[] = {64, 65, 66, 67, 68, 69, 70, 71, 72, 80, 81, 82, 83, 84, 85, 86, 87, 88};
	uint32_t game_over_start_x = center_x - 4;
	uint32_t game_over_y = center_y;
	
	// Draw first row (tiles 64-72) with palette 7
	for (int i = 0; i < 9; ++i) {
		uint32_t bg_x = game_over_start_x + i;
		uint32_t bg_y = game_over_y;
		uint32_t bg_index = bg_x + bg_y * PPU466::BackgroundWidth;
		if (bg_index < ppu.background.size() && bg_x < PPU466::BackgroundWidth && bg_y < PPU466::BackgroundHeight) {
			ppu.background[bg_index] = game_over_tiles[i] | (7 << 8);
		}
	}
	
	// Draw second row (tiles 80-88) with palette 5
	for (int i = 0; i < 9; ++i) {
		uint32_t bg_x = game_over_start_x + i;
		uint32_t bg_y = game_over_y + 1; // One row below
		uint32_t bg_index = bg_x + bg_y * PPU466::BackgroundWidth;
		if (bg_index < ppu.background.size() && bg_x < PPU466::BackgroundWidth && bg_y < PPU466::BackgroundHeight) {
			ppu.background[bg_index] = game_over_tiles[i + 9] | (5 << 8); 
		}
	}
}

void PlayMode::restart_game() {
	std::cout << "Restarting game..." << std::endl;
	
	player_health = 3;
	player_at = glm::vec2(0.0f, 0.0f);  // Reset to starting position
	player_facing_right = false;
	invulnerability_timer = 0.0f;
	game_over = false;
	
	// Reload the level (this clears enemies and collectibles and recreates them)
	load_level_from_map(get_level_map());
}

std::vector<std::string> PlayMode::get_level_map() {
	return {
		"................................",  
		"................................",  
		".................................", 
		"................U...............",  
		"................................",  
		"...L.....................P......",  
		"................U...############",  
		"................................",  
		".....L..........................",  
		"###......###....................",  
		"................................",  
		"................................",  
		".P..........U...................",  
		"############....................",  
		"................................",  
		"................................",  
		"................................",  
		"..U....L.L.L..L.................",  
		"................................",  
		"...........................P....",  
		".................###############",  
		"................................",  
		"................................", 
		"................................",  
		".......................L........",  
		".P..............................",  
		"###############.................",  
		"................................",  
		"........................P.......",  
		".................###############"  
	};
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
