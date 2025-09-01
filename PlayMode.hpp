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
	} left, right, down, up;

	//some weird background animation:
	float background_fade = 0.0f;

	//player position:
	glm::vec2 player_at = glm::vec2(0.0f);
	glm::vec2 player_velocity = glm::vec2(0.0f);
	bool player_facing_right = false;  // false = facing left, true = facing right

	//game entities:
	struct Enemy {
		glm::vec2 position;
		glm::vec2 velocity;
		bool active = true;
		SpriteAnimator animator;
	};
	std::vector<Enemy> enemies;
	
	// Level elements
	struct WoodBlock {
		glm::vec2 position;
		glm::vec2 size = {8.0f, 8.0f};  // 8x8 pixels
	};
	std::vector<WoodBlock> wood_shelves;
	
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
	void create_level_elements();
	bool check_collision(glm::vec2 position, glm::vec2 size);
	void flip_tile_horizontally(PPU466::Tile &tile);
};
