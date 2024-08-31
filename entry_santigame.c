#include "range.c"
#include "entry_santigame.h"


// 0 -> 1
float sin_breathe(float time, float rate) {
	return ((sin(time * rate) + 1.0) / 2.0);
}

bool almost_equals(float a, float b, float epsilon) {
	return fabs(a - b) <= epsilon;
}

bool animate_f32_to_target(float* value, float target, float delta_t, float rate) {
	*value += (target - *value) * (1.0 - pow(2.0f, -rate * delta_t));
	if (almost_equals(*value, target, 0.001f))
	{
		*value = target;
		return true; // reached
	}
	return false;
}

void animate_v2_to_target(Vector2* value, Vector2 target, float delta_t, float rate) {
	animate_f32_to_target(&(value->x), target.x, delta_t, rate);
	animate_f32_to_target(&(value->y), target.y, delta_t, rate);
}


// ^^ generic utils

float player_speed = 100;

#define MAX_ENTITY_COUNT 1024

const int tile_width = 8;
const float entity_selection_radius = 16.0f;

const int tree_health = 3;
const int flower_health = 3;

int world_pos_to_tile_pos(float world_pos) {
	return roundf(world_pos / (float)tile_width);
}

float tile_pos_to_world_pos(int tile_pos) {
	return ((float)tile_pos * (float)tile_width);
}

Vector2 round_v2_to_tile(Vector2 world_pos) {
	world_pos.x = tile_pos_to_world_pos(world_pos_to_tile_pos(world_pos.x));
	world_pos.y = tile_pos_to_world_pos(world_pos_to_tile_pos(world_pos.y));
	return world_pos;
}

typedef struct Sprite {
	Gfx_Image* image;
}	Sprite;

typedef enum SpriteID {
	SPRITE_nil,
	SPRITE_player,
	SPRITE_tree0,
	SPRITE_tree1,
	SPRITE_flower0,
	SPRITE_flower1,
	SPRITE_rock,

	SPRITE_item_pine_wood,
	SPRITE_item_cyan_pigment,
	SPRITE_item_magenta_pigment,
	SPRITE_item_rock,
	SPRITE_paintbrush,
	SPRITE_MAX,
}SpriteID;
Sprite sprites[SPRITE_MAX];

Sprite* get_sprite(SpriteID id) {
	if (id >= 0 && id < SPRITE_MAX) {
		return &sprites[id];
	}
	return &sprites[0];
}

Vector2 get_sprite_size(Sprite* sprite) {
	return (Vector2) { sprite->image->width, sprite->image->height};
}


typedef enum EntityArchetype {
	arch_nil = 0,
	arch_player = 1,
	arch_rock = 2,
	arch_tree = 3,
	arch_flower = 4,

	arch_item_pine_wood = 5,
	arch_item_cyan_pigment = 6,
	arch_item_magenta_pigment = 7,
	arch_item_rock = 8,
	arch_MAX,
} EntityArchetype;


typedef struct Entity {
	bool is_valid;
	EntityArchetype arch;
	Vector2 pos;
	bool render_sprite;
	SpriteID sprite_id;
	int health;
	bool detroyable_world_item;
	bool is_item;
} Entity;

typedef struct World {
	Entity entities[MAX_ENTITY_COUNT];
} World;
World* world = 0;

typedef struct WoldFrame {
	Entity* selected_entity;
} WorldFrame;
WorldFrame world_frame;



Entity* entity_create() {
	Entity* entity_found = 0;
	for (int i= 0; i < MAX_ENTITY_COUNT; i++) {
		Entity* existing_entity = &world->entities[i];
		if (!existing_entity->is_valid){
			entity_found = existing_entity;
			break;
		}	
	}
	assert(entity_found, "No more free entities!");
	entity_found->is_valid = true;
	return entity_found;
}

void entity_destroy(Entity* entity) {
	memset(entity, 0, sizeof(Entity));
}

void setup_flower(Entity* en) {
	en->arch = arch_flower;
	en->sprite_id = SPRITE_flower0;
	//en->sprite_id = SPRITE_flower1;
	en->health = flower_health;
	en->detroyable_world_item = true;
}

void setup_tree(Entity* en) {
	en->arch = arch_tree;
	en->sprite_id = SPRITE_tree0;
	//en->sprite_id = SPRITE_tree1;
	en->health = tree_health;
	en->detroyable_world_item = true;
}

void setup_item_pinewood(Entity* en) {
	en->arch = arch_item_pine_wood;
	en->sprite_id = SPRITE_item_pine_wood;
	en->is_item = true;
}

void setup_item_rock(Entity* en) {
	en->arch = arch_item_rock;
	en->sprite_id = SPRITE_item_rock;
	en->is_item = true;
}

void setup_item_flower0(Entity* en) {
	en->arch = arch_item_cyan_pigment;
	en->sprite_id = SPRITE_item_cyan_pigment;
	en->is_item = true;
}

void setup_item_flower1(Entity* en) {
	en->arch = arch_item_magenta_pigment;
	en->sprite_id = SPRITE_item_magenta_pigment;
	en->is_item = true;
}

void setup_player(Entity* en) {
	en->arch = arch_player;
	en->sprite_id = SPRITE_player;
}

Vector2 screen_to_world() {
	float mouseX = input_frame.mouse_x;
	float mouseY = input_frame.mouse_y;
	Matrix4 projection = draw_frame.projection;
	Matrix4 view = draw_frame.camera_xform;
	float windowWidth = window.width;
	float windowHeight = window.height;

	//Normalize the mouse coordinates
	float ndcX = (mouseX / (windowWidth * 0.5f)) - 1.0f;
	float ndcY = (mouseY / (windowHeight * 0.5f)) - 1.0f;

	//Transform to world coordinates
	Vector4 worldPos = v4(ndcX, ndcY, 0, 1);
	worldPos = m4_transform(m4_inverse(projection), worldPos);
	worldPos = m4_transform(view, worldPos);
	//log("%f, %f", worldPos.x, worldPos.y);
	
	//Return as 2D vector
	return (Vector2){ worldPos.x, worldPos.y};
}

float time_since_destroyed_entity = 0.0f;

int entry(int argc, char **argv) {

	window.title = fixed_string("El Santi Juego");
	window.width = 1280; // We need to set the scaled size if we want to handle system scaling (DPI)
	window.height = 720; 
	window.x = 200;
	window.y = 200;
	window.clear_color = hex_to_rgba(0x272736ff);

	world = alloc(get_heap_allocator(), sizeof(World));
	memset(world, 0, sizeof(World));

    SetSprites();
    Gfx_Font *font = load_font_from_disk(STR("C:/windows/fonts/arial.ttf"), get_heap_allocator());
	assert(font, "Failed loading arial.ttf, %d", GetLastError());
	const u32 font_height = 48;

	Entity* player_en = entity_create();
	setup_player(player_en);
	
	int entity_counter_flower = 0;
	int current_entity_counter_flower = 0;
	int entity_counter_tree = 0;

	const int MAX_FLOWER_ENTITY_AMOUNT = 10;
	const int MAX_TREE_ENTITY_AMOUNT = 10;

	for (int i = 0; i < MAX_FLOWER_ENTITY_AMOUNT; i++){
		Entity* en = entity_create();
		//entity_counter_flower = i;
		setup_flower(en);
		en->pos = v2(get_random_float32_in_range(-200, 200), get_random_float32_in_range(-200, 200));
		en->pos = round_v2_to_tile(en->pos);
		//en->pos.y -= tile_width * 0.5;
		//log("There are: %i, flowers.", (entity_counter_flower + 1));
	}
	for (int i = 0; i < MAX_TREE_ENTITY_AMOUNT; i++){
		Entity* en = entity_create();
		//entity_counter_tree = i;
		setup_tree(en);
		en->pos = v2(get_random_float32_in_range(-200, 200), get_random_float32_in_range(-200, 200));
		en->pos = round_v2_to_tile(en->pos);
		//en->pos.y -= tile_width * 0.5;
		//log("There are: %i, trees.", (entity_counter_tree + 1));
	}
	
	float64 seconds_counter = 0.0;
	s32 frame_count = 0;

	float zoom = 5.3;
	Vector2 camera_pos = v2(0, 0);


	float64 last_time = os_get_elapsed_seconds();


	
	while (!window.should_close) {
		reset_temporary_storage();
		world_frame = (WorldFrame){0};

		float64 now = os_get_elapsed_seconds();
		float64 delta_t = now - last_time;
		last_time = now;
		float time_since_destroyed_entity = time_since_destroyed_entity + delta_t;

		os_update(); 
		
		draw_frame.projection = m4_make_orthographic_projection(window.width * -0.5, window.width * 0.5, window.height * -0.5, window.height * 0.5, -1, 10);
		
		// :camera
		{
			Vector2 target_pos = player_en->pos;
			animate_v2_to_target(&camera_pos, target_pos, delta_t, 30.0f);

			draw_frame.camera_xform = m4_make_scale(v3(1.0, 1.0, 1.0));
			draw_frame.camera_xform = m4_mul(draw_frame.camera_xform, m4_make_translation(v3(camera_pos.x, camera_pos.y, 0)));
			draw_frame.camera_xform = m4_mul(draw_frame.camera_xform, m4_make_scale(v3(1.0/zoom, 1.0/zoom, 1.0)));
		}

		
		Vector2 mouse_pos_world = screen_to_world();
		int mouse_tile_x = world_pos_to_tile_pos(mouse_pos_world.x);
		int mouse_tile_y = world_pos_to_tile_pos(mouse_pos_world.y); 


		Entity* selected_entity;
		// mouse pos in wold space
		{
			//draw_text(font, sprint(get_temporary_allocator(), "%f %f" , mouse_pos_world.x, mouse_pos_world.y), font_height, mouse_pos_world, v2(0.1, 0.1), COLOR_RED);

			float smallest_dist = 0.000000001;
			

			// :selected entity system		
			for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
				Entity* en = &world->entities[i];
				EntityArchetype entity_archetype_selected = en->arch;
				
				if(en->is_valid && entity_archetype_selected > 1 && en->detroyable_world_item) {
					Sprite* sprite = get_sprite(en->sprite_id);

					int entity_tile_x = world_pos_to_tile_pos(en->pos.x);
					int entity_tile_y = world_pos_to_tile_pos(en->pos.y);

					float dist = fabsf(v2_dist(en->pos, mouse_pos_world));
					if (dist < entity_selection_radius) {
						if (!world_frame.selected_entity || (dist < smallest_dist)) {
							world_frame.selected_entity = en;
							smallest_dist = dist;
						}
					}
				}										
			}				
		}
		
		// :tile rendering system
		{
			int player_tile_x = world_pos_to_tile_pos(player_en->pos.x);
			int player_tile_y = world_pos_to_tile_pos(player_en->pos.y);
			int tile_radius_x = 40;
			int tile_radius_y = 30;

			for (int x = player_tile_x - tile_radius_x; x < player_tile_x + tile_radius_x; x++) {
				for (int y = player_tile_y - tile_radius_y; y < player_tile_y + tile_radius_y; y++) {	
					if ((x + (y % 2 == 0)) % 2 == 0) {
						Vector4 selected_tile_color = v4(0.1, 0.1, 0.1, 0.1);
						float x_pos = x * tile_width;
						float y_pos = y * tile_width;
						draw_rect(v2(x_pos + tile_width * -0.5, y_pos + tile_width * -0.5), v2(tile_width, tile_width), selected_tile_color);
						}
					}
				}
		
		//	draw_rect(v2(tile_pos_to_world_pos(mouse_tile_x) + tile_width * -0.5, tile_pos_to_world_pos(mouse_tile_y) + tile_width * -0.5), v2(tile_width, tile_width), v4(0.5, 0.5, 0.5, 0.5));
		}

		// :clicky thingy
		{
			Entity* selected_en = world_frame.selected_entity;

			if(is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
				consume_key_just_pressed(MOUSE_BUTTON_LEFT);

				if(selected_en) {
					selected_en->health -= 1;
					if(selected_en->health <= 0) {

						switch (selected_en->arch) {
						case arch_flower: {
							{
							Entity* en = entity_create();
							setup_item_flower0(en);
							en->pos = selected_en->pos;
							}
						}
						break;
						case arch_tree: {
							{
							Entity* en = entity_create();
							setup_item_pinewood(en);
							en->pos = selected_en->pos;
							}
						}
						break;
						case arch_rock: {
							{
							Entity* en = entity_create();
							setup_item_rock(en);
							en->pos = selected_en->pos;
							}
						}
						break;
						default: {}	break;
						}

						entity_destroy(selected_en);
					}
				}
			}
		}


		//render
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			if(en->is_valid) {
				switch (en->arch) {
					
					default:
					{
						Sprite* sprite = get_sprite(en->sprite_id);
						Matrix4 xform = m4_scalar(1.0);
						if(en->is_item) {
							xform = m4_translate(xform, v3(0, 2.0 * sin_breathe(os_get_elapsed_seconds(), 10.0), 0));
						}

						xform         = m4_translate(xform, v3(0, tile_width * -0.5, 0));
						xform         = m4_translate(xform, v3(en->pos.x, en->pos.y, 0));
						xform         = m4_translate(xform, v3(sprite->image->width * -0.5, 0.0, 0));

						Vector4 col = COLOR_WHITE;
						if (world_frame.selected_entity == en) {
							col = COLOR_RED;
						}

						draw_image_xform(sprite->image, xform, get_sprite_size(sprite), col);

						// debug pos
						//draw_text(font, sprint(get_temporary_allocator(), "%f %f" , en->pos.x, en->pos.y), font_height, en->pos, v2(0.1, 0.1), COLOR_GREEN);
					
					break;					
					}					
				}
			}
		}


		// input		
		{
		if (is_key_just_pressed(KEY_ESCAPE)) {
			window.should_close = true;
		}
		

		Vector2 input_axis = v2(0, 0);
		if (is_key_down('A')) {
			input_axis.x -= 1.0;
		}
		if (is_key_down('D')) {
			input_axis.x += 1.0;
		}
		if (is_key_down('S')) {
			input_axis.y -= 1.0;
		}
		if (is_key_down('W')) {
			input_axis.y += 1.0;
		}

		input_axis = v2_normalize(input_axis);
		
		player_en->pos = v2_add(player_en->pos, v2_mulf(input_axis, player_speed * delta_t));

		gfx_update();
		seconds_counter += delta_t;
		frame_count += 1;
		if (seconds_counter > 1.0) {
			log("fps: %i", frame_count);
			seconds_counter = 0.0;
			frame_count = 0;
		}
		}
	}

	return 0;
}

void SetSprites()
{
    sprites[SPRITE_player] = (Sprite){.image = load_image_from_disk(STR("res/sprites/player.jpeg"), get_heap_allocator())};
    sprites[SPRITE_tree0] = (Sprite){.image = load_image_from_disk(STR("res/sprites/tree00.jpeg"), get_heap_allocator())};
    sprites[SPRITE_tree1] = (Sprite){.image = load_image_from_disk(STR("res/sprites/tree01.jpeg"), get_heap_allocator())};
    sprites[SPRITE_flower0] = (Sprite){.image = load_image_from_disk(STR("res/sprites/flower00.jpeg"), get_heap_allocator())};
    sprites[SPRITE_flower1] = (Sprite){.image = load_image_from_disk(STR("res/sprites/flower01.jpeg"), get_heap_allocator())};
    sprites[SPRITE_item_pine_wood] = (Sprite){.image = load_image_from_disk(STR("res/sprites/pineWoodItem.jpeg"), get_heap_allocator())};
    sprites[SPRITE_item_cyan_pigment] = (Sprite){.image = load_image_from_disk(STR("res/sprites/cyanPigmentItem.jpeg"), get_heap_allocator())};
    sprites[SPRITE_item_magenta_pigment] = (Sprite){.image = load_image_from_disk(STR("res/sprites/flower1_item.jpeg"), get_heap_allocator())};
    sprites[SPRITE_item_rock] = (Sprite){.image = load_image_from_disk(STR("res/sprites/rock_item.jpeg"), get_heap_allocator())};
    sprites[SPRITE_paintbrush] = (Sprite){.image = load_image_from_disk(STR("res/sprites/paintbrush.jpeg"), get_heap_allocator())};
}