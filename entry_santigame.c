#define MAX_ENTITY_COUNT 1024

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
	assert(entity_found != NULL && "No more free entities!");
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

int entry(int argc, char **argv) {

	window.title = fixed_string("El Santi Juego");
	window.width = 1280; // We need to set the scaled size if we want to handle system scaling (DPI)
	window.height = 720; 
	window.x = 200;
	window.y = 90;
	window.clear_color = hex_to_rgba(0x272736ff);

	world = alloc(get_heap_allocator(), sizeof(World));
	memset(world, 0, sizeof(World));

	sprites[SPRITE_player] = (Sprite){ .image=load_image_from_disk(STR("player.png"), get_heap_allocator()), .size=v2(6.0, 8.0) };
	sprites[SPRITE_tree0] = (Sprite){ .image=load_image_from_disk(STR("tree0.png"), get_heap_allocator()), .size=v2(9.0, 16.0) };
	 if (sprites[SPRITE_tree0].image == NULL) {
        log("Error loading tree0.png");
    }
	sprites[SPRITE_tree1] = (Sprite){ .image=load_image_from_disk(STR("tree1.png"), get_heap_allocator()), .size=v2(9.0, 14.0) };
	sprites[SPRITE_flower0] = (Sprite){ .image=load_image_from_disk(STR("flower0.png"), get_heap_allocator()), .size=v2(5.0, 7.0) };
	if (sprites[SPRITE_flower0].image == NULL) {
        log("Error loading flower0.png");
    }

	Entity* player_en = entity_create();
	setup_player(player_en);
	
	for (int i = 0; i < 10; i++){
		Entity* en = entity_create();
		setup_flower(en);
		en->pos = v2(get_random_float32_in_range(-1, 1), get_random_float32_in_range(-1, 1));
	}
	for (int i = 0; i < 10; i++){
		Entity* en = entity_create();
		setup_tree(en);
		en->pos = v2(get_random_float32_in_range(-1, 1), get_random_float32_in_range(-1, 1));
	}
	
	float64 seconds_counter = 0.0;
	s32 frame_count = 0;

	float64 last_time = os_get_elapsed_seconds();

	while (!window.should_close) {
		reset_temporary_storage();
		
		draw_frame.projection = m4_make_orthographic_projection(window.width * -0.5, window.width * 0.5, window.height * -0.5, window.height * 0.5, -1, 10);

		float zoom = 5.3;
		draw_frame.camera_xform = m4_make_scale(v3(1.0/zoom, 1.0/zoom, 1.0));

		float64 now = os_get_elapsed_seconds();
		float64 delta_t = now - last_time;
		last_time = now;

		os_update(); 
		//render
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			if(en->is_valid) {
				switch (en->arch) {
					
					default:
					{
						Sprite* sprite = get_sprite(en->sprite_id);
						Matrix4 xform = m4_scalar(1.0);
						xform         = m4_translate(xform, v3(en->pos.x, en->pos.y, 0));
						xform         = m4_translate(xform, v3(sprite->size.x * -0.5, 0.0, 0));
						draw_image_xform(sprite->image, xform, sprite->size, COLOR_WHITE);
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
		
		player_en->pos = v2_add(player_en->pos, v2_mulf(input_axis, 30.0 * delta_t));

		Vector2 size = v2(6.0, 8.0);
		Matrix4 xform = m4_scalar(1.0);
		xform         = m4_translate(xform, v3(player_en->pos.x, player_en->pos.y, 0));
		xform         = m4_translate(xform, v3(size.x * -0.5, 0.0, 0));

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