#include "PPU466.hpp"
#include "Mode.hpp"
#include "Sprites.hpp"

#include <glm/glm.hpp>

#include <vector>
#include <deque>

struct PlayMode : Mode {
	PlayMode();
	virtual ~PlayMode();

	//functions called by main loop:
	virtual bool handle_event(SDL_Event const &, glm::uvec2 const &window_size) override;
	virtual void update(float elapsed) override;
	virtual void draw(glm::uvec2 const &drawable_size) override;

	//----- game state -----

	//input tracking:
	struct Button {
		uint8_t downs = 0;
		uint8_t pressed = 0;
	} left, right, down, up, action;

	//some weird background animation:
	float background_fade = 0.0f;

	//player position:
	glm::vec2 player_at = glm::vec2(0.0f);
	glm::vec2 player_velocity = glm::vec2(0.0f);
	bool player_facing_right = false;  // false = facing left, true = facing right
	
	//player health:
	int player_health = 3;
	float invulnerability_timer = 0.f;  // so that player isn't taking damage too quickly
	
	//game state:
	bool game_over = false;

	//Enemey will move:
	struct Enemy {
		glm::vec2 position;
		glm::vec2 velocity;
		glm::vec2 start_position;     // Starting position for path
		glm::vec2 move_direction;     // Direction of movement (normalized)
		float path_distance = 16.0f; // How far to move in pixels
		float current_distance = 0.0f; // Current distance traveled
		bool moving_forward = true;   // Direction on the path
		bool active = true;
		SpriteAnimator animator;
	};
	std::vector<Enemy> enemies;
	
	// Level elements - wood blocks are now stored as background tile positions
	std::vector<glm::ivec2> wood_tile_positions;  // Tile coordinates (not pixel coordinates)
	
	struct Collectible {
		glm::vec2 position;
		std::string type;  // "heart", "pot", "flower"
		bool collected = false;
	};
	std::vector<Collectible> collectibles;

	//sprite management:
	Sprites sprites;                    // All sprite definitions
	SpriteAnimator player_animator;     // Player animation
	uint32_t next_sprite_slot = 0;

	//----- drawing handled by PPU466 -----

	PPU466 ppu;

	void create_game_sprites();
	void spawn_enemy(glm::vec2 position);
	uint32_t allocate_sprite_slots(uint32_t count);
	void create_level_background();
	void load_level_from_map(const std::vector<std::string> &level_map);
	void update_background_with_wood();
	bool check_collision(glm::vec2 position, glm::vec2 size);
	void check_pot_interaction();
	void check_enemy_collisions();
	void spawn_enemy_with_direction(glm::vec2 position, glm::vec2 direction, float distance = 16.0f);
	void draw_game_over_screen();
	void restart_game();
	std::vector<std::string> get_level_map();
	void flip_tile_horizontally(PPU466::Tile &tile);
};
