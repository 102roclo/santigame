#include "range.c"

#define MAX_ENTITY_COUNT 1024
float player_speed = 100;

const int tile_width = 8;

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


typedef struct Sprite {
	Gfx_Image* image;
	Vector2 size;
}	Sprite;

typedef enum SpriteID {
	SPRITE_nil,
	SPRITE_player,
	SPRITE_tree0,
	SPRITE_tree1,
	SPRITE_flower0,
	SPRITE_rock,
	SPRITE_MAX,
}SpriteID;
Sprite sprites[SPRITE_MAX];

Sprite* get_sprite(SpriteID id) {
	if (id >= 0 && id < SPRITE_MAX) {
		return &sprites[id];
	}
	return &sprites[0];
}

typedef enum EntityArchetype {
	arch_nil = 0,
	arch_player = 1,
	arch_rock = 2,
	arch_tree = 3,
	arch_flower = 4,
} EntityArchetype;


typedef struct Entity {
	bool is_valid;
	EntityArchetype arch;
	Vector2 pos;
	bool render_sprite;
	SpriteID sprite_id;
} Entity;

typedef struct World {
	Entity entities[MAX_ENTITY_COUNT];
} World;
World* world = 0;

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
}

void setup_tree(Entity* en) {
	en->arch = arch_tree;
	en->sprite_id = SPRITE_tree0;
	//en->sprite_id = SPRITE_tree1;
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



int entry(int argc, char **argv) {

	window.title = fixed_string("El Santi Juego");
	window.width = 1280; // We need to set the scaled size if we want to handle system scaling (DPI)
	window.height = 720; 
	window.x = 200;
	window.y = 200;
	window.clear_color = hex_to_rgba(0x272736ff);

	world = alloc(get_heap_allocator(), sizeof(World));
	memset(world, 0, sizeof(World));

	sprites[SPRITE_player] = (Sprite){ .image=load_image_from_disk(STR("player.png"), get_heap_allocator()), .size=v2(6.0, 8.0) };
	sprites[SPRITE_tree0] = (Sprite){ .image=load_image_from_disk(STR("tree0.jpeg"), get_heap_allocator()), .size=v2(9.0, 16.0) };
	sprites[SPRITE_tree1] = (Sprite){ .image=load_image_from_disk(STR("tree1.jpeg"), get_heap_allocator()), .size=v2(9.0, 14.0) };
	sprites[SPRITE_flower0] = (Sprite){ .image=load_image_from_disk(STR("flower0.jpeg"), get_heap_allocator()), .size=v2(5.0, 7.0) };
	
	Gfx_Font *font = load_font_from_disk(STR("C:/windows/fonts/arial.ttf"), get_heap_allocator());
	assert(font, "Failed loading arial.ttf, %d", GetLastError());
	const u32 font_height = 48;

	Entity* player_en = entity_create();
	setup_player(player_en);
	
	for (int i = 0; i < 10; i++){
		Entity* en = entity_create();
		setup_flower(en);
		en->pos = v2(get_random_float32_in_range(-200, 200), get_random_float32_in_range(-200, 200));
		en->pos = round_v2_to_tile(en->pos);
		en->pos.y -= tile_width * 0.5;
	}
	for (int i = 0; i < 10; i++){
		Entity* en = entity_create();
		setup_tree(en);
		en->pos = v2(get_random_float32_in_range(-200, 200), get_random_float32_in_range(-200, 200));
		en->pos = round_v2_to_tile(en->pos);
		en->pos.y -= tile_width * 0.5;
	}
	
	float64 seconds_counter = 0.0;
	s32 frame_count = 0;

	float zoom = 5.3;
	Vector2 camera_pos = v2(0, 0);


	float64 last_time = os_get_elapsed_seconds();
	while (!window.should_close) {
		reset_temporary_storage();

		float64 now = os_get_elapsed_seconds();
		float64 delta_t = now - last_time;
		last_time = now;
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

		
		Vector2 mouse_pos = screen_to_world();
		int mouse_tile_x = world_pos_to_tile_pos(mouse_pos.x);
		int mouse_tile_y = world_pos_to_tile_pos(mouse_pos.y); 
		// mouse pos in wold space
		{
			//draw_text(font, sprint(get_temporary_allocator(), "%f %f" , mouse_pos.x, mouse_pos.y), font_height, mouse_pos, v2(0.1, 0.1), COLOR_RED);

			// :selected entity system		
			for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
				Entity* en = &world->entities[i];
				EntityArchetype entity_archetype_selected = en->arch;
				
				if(en->is_valid && entity_archetype_selected > 1) {
					Sprite* sprite = get_sprite(en->sprite_id);
					Range2f bounds = range2f_make_bottom_center(sprite->size);
					bounds = range2f_shift(bounds, en->pos);
					
					Vector4 entity_highlight_color = COLOR_RED;
					entity_highlight_color.a = 0.4;
					if (range2f_contains(bounds, mouse_pos)) {
						entity_highlight_color.a = 0.85;
					}

					draw_rect(bounds.min, range2f_size(bounds), entity_highlight_color);
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
		
			draw_rect(v2(tile_pos_to_world_pos(mouse_tile_x) + tile_width * -0.5, tile_pos_to_world_pos(mouse_tile_y) + tile_width * -0.5), v2(tile_width, tile_width), v4(0.5, 0.5, 0.5, 0.5));
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
						xform         = m4_translate(xform, v3(0, (float)tile_width * -0.5, 0));
						xform         = m4_translate(xform, v3(en->pos.x, en->pos.y, 0));
						xform         = m4_translate(xform, v3(sprite->size.x * -0.5, 0.0, 0));
						draw_image_xform(sprite->image, xform, sprite->size, COLOR_WHITE);

						draw_text(font, sprint(get_temporary_allocator(), "%f %f" , en->pos.x, en->pos.y), font_height, en->pos, v2(0.1, 0.1), COLOR_GREEN);
					
					break;					
					}
					
				}
			}
		}

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

	return 0;
}