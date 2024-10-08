#include "range.c"

#define m4_identity  m4_make_scale(v3(1, 1, 1))

// ^^^ engine changes

// the scuff zone

float float_alpha(float x, float min, float max) {
	float res = (x-min) / (max-min);
	res = clamp(res, 0.0, 1.0);
	return res;
}

inline float64 now() {
	return os_get_current_time_in_seconds();
}

float alpha_from_end_time(float64 end_time, float length) {
	return float_alpha(now(), end_time-length, end_time);
}

bool has_reached_end_time(float64 end_time) {
	return now() > end_time;
}

Draw_Quad ndc_quad_to_screen_quad(Draw_Quad ndc_quad){
	
	// NOTE: assume this are the screen space matrices.
	Matrix4 proj = draw_frame.projection;
	Matrix4 view = draw_frame.camera_xform;

	Matrix4 ndc_to_screen_space = m4_identity;
	ndc_to_screen_space = m4_mul(ndc_to_screen_space, m4_inverse(proj));
	ndc_to_screen_space = m4_mul(ndc_to_screen_space, view);

	ndc_quad.bottom_left = m4_transform(ndc_to_screen_space, v4(v2_expand(ndc_quad.bottom_left), 0, 1)).xy;
	ndc_quad.bottom_right = m4_transform(ndc_to_screen_space, v4(v2_expand(ndc_quad.bottom_right), 0, 1)).xy;
	ndc_quad.top_left = m4_transform(ndc_to_screen_space, v4(v2_expand(ndc_quad.top_left), 0, 1)).xy;
	ndc_quad.top_right = m4_transform(ndc_to_screen_space, v4(v2_expand(ndc_quad.top_right), 0, 1)).xy;

	return ndc_quad;
}

// resizable array helpers

#define MAX_RECIPE_INGREDIENTS 8

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

Range2f quad_to_range(Draw_Quad quad) {
	return (Range2f){quad.bottom_left, quad.top_right};
}


// ^^ generic utils

// :tweaks

Vector4 bg_box_color = {0, 0, 0, 0.5};
float player_speed = 100;
#define MAX_ENTITY_COUNT 1024
const int tile_width = 8;
const float entity_selection_radius = 16.0f;
const float player_pickup_radius = 20.0f;
const int tree_health = 3;
const int flower_health = 3;
const int rock_health = 3;
const s32 layer_ui = 20;
const s32 layer_world = 20;

// :global app stuff

float64 delta_t;
Gfx_Font* font;
u32 font_height = 48;

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
	SPRITE_furnace,
	SPRITE_workbench,
	SPRITE_paintStation,	
	SPRITE_MAX,
}SpriteID;
Sprite sprites[SPRITE_MAX];

Sprite* get_sprite(SpriteID id) {
	if (id >= 0 && id < SPRITE_MAX) {
		Sprite* sprite = &sprites[id];
		if(sprite->image) {
			return sprite;
		} else {
			return &sprites[0];
			}
	}
	return &sprites[0];
}


Vector2 get_sprite_size(Sprite* sprite) {
	return (Vector2) { sprite->image->width, sprite->image->height};
}

typedef enum ItemID {
	ITEM_nil,
	ITEM_rock,
	ITEM_pine_wood,
	ITEM_cyan_pigment,
	ITEM_magenta_pigment,
	// :item
	ITEM_MAX,
} ItemID;
typedef struct InventoryItemData {
	int amount;
} InventoryItemData; 

typedef struct ItemAmount {
	ItemID id;
	int amount;
} ItemAmount; 


typedef enum EntityArchetype {
	ARCH_nil = 0,
	ARCH_player = 1,
	ARCH_rock = 2,
	ARCH_tree = 3,
	ARCH_flower = 4,
	ARCH_item = 5,
	ARCH_furnace = 6,
	ARCH_workbench = 7,
	ARCH_paint_station = 8,

	ARCH_MAX,
} EntityArchetype;

typedef struct ItemData {
	string pretty_name;
	// :recipe crafting
	EntityArchetype for_structure;
	ItemAmount crafting_recipe[MAX_RECIPE_INGREDIENTS];
	float craft_length;
} ItemData;
ItemData item_data[ITEM_MAX];
ItemData get_item_data(ItemID id) {
	return item_data[id];
}
int get_crafting_recipe_count(ItemData item_data){
	int count = 0;
	for (int i = 0; i < MAX_RECIPE_INGREDIENTS; i++) {
		if(item_data.crafting_recipe[i].id == 0) {
			break;
		}
		count += 1;
	}
	return count;
}



/*SpriteID get_sprite_id_from_archetype(EntityArchetype arch ) {
	switch (arch)
	{
	case ITEM_pine_wood: return SPRITE_item_pine_wood;	break;
	case ITEM_cyan_pigment: return SPRITE_item_cyan_pigment;break;
	case ITEM_magenta_pigment: return SPRITE_item_magenta_pigment;	break;
	case ITEM_rock: return SPRITE_item_rock;	break;

	default: return 0;
	}
}
*/

typedef struct Entity {
	bool is_valid;
	EntityArchetype arch;
	ItemID item_id;
	Vector2 pos;
	bool render_sprite;
	SpriteID sprite_id;
	int health;
	bool detroyable_world_item;
	bool is_item;
	bool workbench_thing;
	ItemID current_crafting_item;
	int current_crafting_amount;
	float64 crafting_end_time;
} Entity;


// :building resource
// NOTE - a "resource" is a thing that we set up during startup, and is constant.
typedef enum BuildingID {
	BUILDING_nil,
	BUILDING_furnace,
	BUILDING_workbench,
	//BUILDING_research_station,
	BUILDING_paint_station,
	BUILDING_MAX,
} BuildingID;

typedef struct BuildingData {
	EntityArchetype to_build;
	SpriteID icon;
	// display nam
	// cost
} BuildingData;

BuildingData buildings[BUILDING_MAX];

BuildingData* get_building_data(BuildingID id) {
	return &buildings[id];
}


// :ux
typedef enum UXState {
	UX_nil,
	UX_inventory,
	UX_building,
	UX_place_mode,
	UX_workbench,
} UXState;

typedef struct World {
	Entity entities[MAX_ENTITY_COUNT];
	InventoryItemData inventory_items[ITEM_MAX];
	UXState ux_state;
	float inventory_alpha;
	float inventory_alpha_target;
	float building_alpha;
	float building_alpha_target;
	BuildingID placing_building;
	Entity* open_workbench;
	ItemID selected_crafting_item;
	// :world :state
} World;
World* world = 0;

typedef struct WorldFrame {
	Entity* selected_entity;
	Matrix4 world_proj;
	Matrix4 world_view;
	//float delta_t;
	bool hover_consumed;
	
} WorldFrame;
WorldFrame world_frame;

SpriteID get_sprite_id_from_item(ItemID item) {
	switch (item) {
		case ITEM_pine_wood: return SPRITE_item_pine_wood; break;
		case ITEM_rock: return SPRITE_item_rock; break;
		case ITEM_cyan_pigment: return SPRITE_item_cyan_pigment; break;
		case ITEM_magenta_pigment: return SPRITE_item_magenta_pigment; break;
		default: return 0;
	}
}

string get_archetype_pretty_name (EntityArchetype arch) {
	switch (arch)
	{
	case ARCH_furnace: return STR("Furnace");
	case ARCH_workbench: return STR("Workbench");
	case ARCH_paint_station: return STR("Paint Station");
	default: return STR("nil");
	}
}

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

// :setup things

void setup_furnace (Entity* en) {
	en->arch = ARCH_furnace;
	en->sprite_id = SPRITE_furnace;
	en->workbench_thing = true;
}

void setup_workbench (Entity* en) {
	en->arch = ARCH_workbench;
	en->sprite_id = SPRITE_workbench;
	en->workbench_thing = true;
}

void setup_paint_station (Entity* en) {
	en->arch = ARCH_paint_station;
	en->sprite_id = SPRITE_paintStation;
	en->workbench_thing = true;
}

void setup_flower(Entity* en) {
	en->arch = ARCH_flower;
	en->sprite_id = SPRITE_flower0;
	//en->sprite_id = SPRITE_flower1;
	en->health = flower_health;
	en->detroyable_world_item = true;
}

void setup_tree(Entity* en) {
	en->arch = ARCH_tree;
	en->sprite_id = SPRITE_tree0;
	//en->sprite_id = SPRITE_tree1;
	en->health = tree_health;
	en->detroyable_world_item = true;
}

void setup_rock(Entity* en) {
	en->arch = ARCH_rock;
	en->sprite_id = SPRITE_rock;
	en->health = rock_health;
	en->detroyable_world_item = true;
}

void setup_item(Entity* en, ItemID item_id) {
	en->arch = ARCH_item;
	en->sprite_id = get_sprite_id_from_item(item_id);
	en->is_item = true;
	en->item_id = item_id;
}

void setup_player(Entity* en) {
	en->arch = ARCH_player;
	en->sprite_id = SPRITE_player;
}

void entity_setup(Entity* en, EntityArchetype id) {
	switch (id) {
		case ARCH_furnace: setup_furnace(en); break;
		case ARCH_workbench: setup_workbench(en); break;
		case ARCH_paint_station: setup_paint_station(en); break;
		default: log_error("missing entity_setup case entry"); break;
	}
}

Vector2 get_mouse_pos_in_ndc() {
	float mouseX = input_frame.mouse_x;
	float mouseY = input_frame.mouse_y;
	Matrix4 projection = draw_frame.projection;
	Matrix4 view = draw_frame.camera_xform;
	float windowWidth = window.width;
	float windowHeight = window.height;

	//Normalize the mouse coordinates
	float ndcX = (mouseX / (windowWidth * 0.5f)) - 1.0f;
	float ndcY = (mouseY / (windowHeight * 0.5f)) - 1.0f;

	return (Vector2){ ndcX, ndcY};
}

Vector2 get_mouse_pos_in_world_space() {
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

float screen_width = 240;
float screen_height = 135.0;		
void set_screen_space() {
	draw_frame.camera_xform = m4_scalar(1.0);
	draw_frame.projection = m4_make_orthographic_projection(0.0, screen_width, 0.0, screen_height, -1, 10);
}

void set_world_space() {
	draw_frame.projection = world_frame.world_proj;
	draw_frame.camera_xform = world_frame.world_view;
}

// :func dump
void do_ui_stuff() {	
	set_screen_space();
	push_z_layer(50);
	

	// "screen space"
		
		// :inventory UI

		if(is_key_just_pressed(KEY_TAB)) {
			consume_key_just_pressed(KEY_TAB);
			world->ux_state = (world->ux_state == UX_inventory ? UX_nil : UX_inventory);
		}
		world->inventory_alpha_target = (world->ux_state == UX_inventory ? 1.0 : 0.0);
		animate_f32_to_target(&world->inventory_alpha, world->inventory_alpha_target, delta_t, 15.0);
		bool is_inventory_enabled = world->inventory_alpha_target == 1.0;
		if(world->inventory_alpha_target != 0.0)	
		{
		// TODO - some opacity thing here
			
		float y_pos = 70.0;
		int item_count = 0;

		for(int i = 0; i < ARCH_MAX; i++ ) {
			InventoryItemData* item = &world->inventory_items[i];
			if (item->amount > 0) {
				item_count += 1;
			}
		}

		const float icon_thing = 8.0;
		float icon_width = icon_thing;
		const int icon_row_count = 8;

		float entire_thing_width = icon_row_count * icon_width;
		float x_start_pos = (screen_width/2.0)-(entire_thing_width/2.0);

		// bg box rendering thingy
		{
			Matrix4 xform = m4_identity;
			xform = m4_translate(xform, v3(x_start_pos, y_pos, 0.0));
			draw_rect_xform(xform, v2(entire_thing_width, icon_width), bg_box_color);
		}
			
		int slot_index = 0;
		for(int i = 1; i < ITEM_MAX; i++ ) {
			ItemData item_data = get_item_data(i);
			InventoryItemData* item = &world->inventory_items[i];
		if (item->amount > 0) {

			float slot_index_offset = slot_index * icon_width;

			Matrix4 xform = m4_scalar(1.0);
			xform = m4_translate(xform, v3(x_start_pos + slot_index_offset, y_pos, 0.0));						
			Sprite* sprite = get_sprite(get_sprite_id_from_item(i));
					
			float is_selected_alpha = 0.0;

			Draw_Quad* quad = draw_rect_xform(xform, v2(8, 8), v4(1, 1, 1, 0.2));
			Range2f icon_box = quad_to_range(*quad);

			if(is_inventory_enabled && range2f_contains(icon_box, get_mouse_pos_in_ndc())) {
				is_selected_alpha = 1.0;
			}
							
			Matrix4 box_bottom_right_xform = xform;

			xform = m4_translate(xform, v3(icon_width * 0.5, icon_width * 0.5, 0.0));

			if(is_selected_alpha == 1.0)
					
			{ 
				float scale_adjust = 0.3; // * sin_breathe(os_get_current_time_in_seconds(), 20.0);
				xform = m4_scale(xform, v3(1 + scale_adjust, 1 + scale_adjust, 1));	
			}
			{
				// could also rotate
				//	float adjust = PI32 * 2 * sin_breathe(os_get_current_time_in_seconds(), 1.0);
				//	xform = m4_rotate_z(xform, adjust);	
			}
			xform = m4_translate(xform, v3(get_sprite_size(sprite).x * -0.5, get_sprite_size(sprite).y * -0.5, 0));
			draw_image_xform(sprite->image, xform, get_sprite_size(sprite), COLOR_WHITE);

			//draw_text_xform(font, STR("5"), font_height, box_bottom_right_xform, v2(0.1, 0.1), COLOR_BLACK);

			// tooltip
			if(is_selected_alpha == 1.0)
			{
				Draw_Quad screen_quad = ndc_quad_to_screen_quad(*quad);
				Range2f screen_range = quad_to_range(screen_quad);
				Vector2 icon_center = range2f_get_center(screen_range);
				//	icon_center
				Matrix4 xform = m4_scalar(1.0);
				// TODO - we guessing at the Y box size here.
				// in order to automate this, we would need to move it down below after we do all the element advance
				// things for the text. 
				// ... but the the box would be drawing in front of everyone. So we´d have to do Z sorting.
				// Solution for now is to just gess at the size and OOGA BOOGA
				Vector2 box_size = v2(40, 14);

				//xform = m4_pivot_box(xform, box_size, PIVOT_top_center);
				xform = m4_translate(xform, v3(box_size.x * -0.5, -box_size.y - icon_width * 0.5, 0));
				xform = m4_translate(xform, v3(icon_center.x, icon_center.y, 0));
				draw_rect_xform(xform, box_size, bg_box_color);

				float current_y_pos = icon_center.y;
				{
				string title = item_data.pretty_name;

				Gfx_Text_Metrics metrics = measure_text(font, title, font_height, v2(0.1, 0.1));
				Vector2 draw_pos = icon_center;
				draw_pos = v2_sub(draw_pos, metrics.visual_pos_min);
				draw_pos = v2_add(draw_pos, v2_mul(metrics.visual_size, v2(-0.5, -1.0))); // top center

				draw_pos = v2_add(draw_pos, v2(0, icon_width * -0.5));
				draw_pos = v2_add(draw_pos, v2(0, -2.0)); // padding

				draw_text(font, title, font_height, draw_pos, v2(0.1, 0.1), COLOR_WHITE);

				current_y_pos = draw_pos.y;
				}
					{
					string text = STR("x%i");
					text = sprint(temp_allocator, text, item->amount);

					Gfx_Text_Metrics metrics = measure_text(font, text, font_height, v2(0.1, 0.1));
					Vector2 draw_pos = v2(icon_center.x, current_y_pos);
					draw_pos = v2_sub(draw_pos, metrics.visual_pos_min);
					draw_pos = v2_add(draw_pos, v2_mul(metrics.visual_size, v2(-0.5, -1.0))); // top center

					draw_pos = v2_add(draw_pos, v2(0, -2.0)); // padding

					draw_text(font, text, font_height, draw_pos, v2(0.1, 0.1), COLOR_WHITE);	
					}

				}
					slot_index += 1;
				}
			}						
		}

		// :building UI
		{
			if(is_key_just_pressed('C')) {
				consume_key_just_pressed('C');
				world->ux_state = (world->ux_state == UX_building ? UX_nil : UX_building);
			}
				world->building_alpha_target = (world->ux_state == UX_building ? 1.0 : 0.0);
				animate_f32_to_target(&world->building_alpha, world->building_alpha_target, delta_t, 15.0);
				bool is_building_enabled = world->building_alpha_target == 1.0;

			// draw a row of icons for buildable structures
			if (world->building_alpha == 1.0) {
				int building_count = BUILDING_MAX - 1;

				float icon_size = 12.0;
				float total_box_width = building_count * icon_size;
			
				float padding = 2.0;
				total_box_width += padding * (building_count + 1);

				for (BuildingID i = 1; i < BUILDING_MAX; i++) {
					BuildingData* building = &buildings[i];

					Matrix4 xform = m4_identity;

					float x0 = (screen_width * 0.5) - (total_box_width * 0.5);
					x0 += (i-1) * icon_size;
					x0 += padding * (i);
					xform = m4_translate(xform, v3(x0, 10, 0));

					Sprite* icon = get_sprite(building->icon);
					Draw_Quad* quad = draw_image_xform(icon->image, xform, v2(icon_size, icon_size), COLOR_WHITE);
					Range2f box = quad_to_range(*quad);
					//draw_rect_xform(xform, v2(icon_size, icon_size), COLOR_WHITE);

					if(range2f_contains(box, get_mouse_pos_in_ndc())) {
						// if hover, do tooltip
						world_frame.hover_consumed = true;

						if(is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
							consume_key_just_pressed(MOUSE_BUTTON_LEFT);
							world->placing_building = i;
							world->ux_state = UX_place_mode;
						}
					}
				}
			}
		}

		// :placement mode
		// TODO - alpha animation in place mode
		if (world->ux_state == UX_place_mode)
		{
			set_world_space();
			{
				Vector2 mouse_pos_world = get_mouse_pos_in_world_space();
				BuildingData* building = get_building_data(world->placing_building);
				Sprite* icon = get_sprite(building->icon);

				Vector2 pos = mouse_pos_world;
				pos = round_v2_to_tile(pos);

				Matrix4 xform = m4_identity;
				xform = m4_translate(xform, v3(pos.x, pos.y, 0));

				// @volatile with enttity rendering
				xform = m4_translate(xform, v3(0, tile_width * -0.5, 0));
				xform = m4_translate(xform, v3(get_sprite_size(icon).x * -0.5, 0.0, 0));

				draw_image_xform(icon->image, xform, get_sprite_size(icon), COLOR_WHITE);
			
				if(is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
					consume_key_just_pressed(MOUSE_BUTTON_LEFT);
					Entity* en = entity_create();
					entity_setup(en, building->to_build);
					en->pos = pos;
					world->ux_state = 0;
				}
			}
			set_screen_space();
		}

	// :workbench ui
	if (world->ux_state == UX_workbench && world->open_workbench) {
		Entity* workbench_en = world->open_workbench;
		
		Vector2 section_size = v2(50.0, 70.0);
		float gap_between_panels = 10.0;
		float text_height_pad = 4.0;
		Vector4 bg_col = v4(0, 0, 0, 0.7);
		Vector4 fill_col = v4(0.5, 0.5, 0.5, 1.0);

		float ui_width_thing = section_size.x * 2.0 + gap_between_panels;

		float x0 = screen_width * 0.5 - ui_width_thing * 0.5;
		float y0 = screen_height * 0.5 - section_size.y * 0.5;
		{
			Matrix4 xform = m4_identity;
			xform = m4_translate(xform, v3(x0, y0, 0));
			draw_rect_xform(xform, section_size, bg_col);

			// title
			float x1 = x0;
			float y1 = y0 + section_size.y;
			{
				string title = get_archetype_pretty_name(workbench_en->arch);
				Gfx_Text_Metrics metrics = measure_text(font, title, font_height, v2(0.1, 0.1));

				float center_pos = x0 + section_size.x * 0.5;
				Vector2 draw_pos = v2(center_pos, y1);
				draw_pos = v2_sub(draw_pos, metrics.visual_pos_min);
				draw_pos = v2_add(draw_pos, v2_mul(metrics.visual_size, v2(-0.5, -1.0))); // top center
				draw_pos.y -= text_height_pad;

				draw_text(font, title, font_height, draw_pos, v2(0.1, 0.1), COLOR_WHITE);

				y1 = draw_pos.y; 
				y1 -= text_height_pad;
			}

			Vector2 item_icon_size = v2(8, 8);

			y1 -= item_icon_size.y;

			// draw all item recipes
			for(int i = 1; i < ITEM_MAX; i++){
				ItemData data = get_item_data(i);
				if (data.for_structure == workbench_en->arch && get_crafting_recipe_count(data)) {
					Matrix4 xform = m4_identity;
					xform = m4_translate(xform, v3(x1, y1, 0));

					Vector4 col = COLOR_WHITE;
					if (world->selected_crafting_item == i) {
						col = COLOR_RED;
					}

					Draw_Quad* quad = draw_image_xform(get_sprite(get_sprite_id_from_item(i))->image, xform, item_icon_size, col);
					Range2f icon_box = quad_to_range(*quad);
					if(range2f_contains(icon_box, get_mouse_pos_in_ndc())) {
						if(is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
							consume_key_just_pressed(MOUSE_BUTTON_LEFT);
							world->selected_crafting_item = i;
						}
					}
					x1 += item_icon_size.x;
				}
			}
		}

		x0 += section_size.x;
		x0 += gap_between_panels;
		Vector2 bottom_left_right_pane = v2(x0, y0);
		Vector2 top_left_right_pane = v2(x0, y0 + section_size.y);

		// right pane
		{
		 	Matrix4 xform = m4_identity;
			xform = m4_translate(xform, v3(x0, y0, 0));
		 	draw_rect_xform(xform, section_size, bg_col);		

			if(world->selected_crafting_item) {
				ItemData selected_item_data = get_item_data(world->selected_crafting_item);

				// title of item
				y0 += section_size.y;
				{
					string title = selected_item_data.pretty_name;
					Gfx_Text_Metrics metrics = measure_text(font, title, font_height, v2(0.1, 0.1));

					float center_pos = x0 + section_size.x * 0.5;
					Vector2 draw_pos = v2(center_pos, x0);
					draw_pos = v2_sub(draw_pos, metrics.visual_pos_min);
					draw_pos = v2_add(draw_pos, v2_mul(metrics.visual_size, v2(-0.5, -1.0))); //top center
					draw_pos.y -= text_height_pad;

					draw_text(font, title, font_height, draw_pos, v2(0.1, 0.1), COLOR_WHITE);

					y0 = draw_pos.y;
					y0 -= text_height_pad;
				}

				y0 = bottom_left_right_pane.y + 30.0;

				// list out the ingredients
				for (int i = 0; i < get_crafting_recipe_count(selected_item_data); i++) {
					ItemAmount ingredient_amount = selected_item_data.crafting_recipe[i];
					ItemData ingredient_item_data = get_item_data(ingredient_amount.id);

					Vector2 element_size = v2(section_size.x * 0.8, 6.0);

					x0 = bottom_left_right_pane.x + (section_size.x - element_size.x) * 0.5;

					// bg box thing
					draw_rect(v2(x0, y0), element_size, fill_col);

					// icon
					{
						float item_icon_length = element_size.y * 0.8;

						float x1 = bottom_left_right_pane.x + section_size.x * 0.5 - element_size.y;
						float y1 = y0 + (item_icon_length - element_size.y) * 0.5;

						Matrix4 xform = m4_identity;
						xform = m4_translate(xform, v3(x1, y1, 0));
						Draw_Quad* quad = draw_image_xform(get_sprite(get_sprite_id_from_item(ingredient_amount.id))->image, xform, v2(item_icon_length, item_icon_length), COLOR_WHITE);
						Range2f icon_box = quad_to_range(*quad);
						if (range2f_contains(icon_box, get_mouse_pos_in_ndc())) {

						}
					}

					InventoryItemData inv_item = world->inventory_items[ingredient_amount.id];
					Vector4 txt_col = COLOR_WHITE;
					if (inv_item.amount < ingredient_amount.amount) {
						txt_col = COLOR_RED;
					}

					string txt = tprint("%i/%i", inv_item.amount, ingredient_amount.amount);
					Gfx_Text_Metrics metrics = measure_text(font, txt, font_height, v2(0.1, 0.1));
					float center_pos = bottom_left_right_pane.x + section_size.x * 0.5;
					Vector2 draw_pos = v2(center_pos, y0 + element_size.y * 0.5);
					draw_pos = v2_sub(draw_pos, metrics.visual_pos_min);
					draw_pos = v2_sub(draw_pos, v2_mul(metrics.visual_size, v2(0, 0.5)));
					draw_text(font, txt, font_height, draw_pos, v2(0.1, 0.1), txt_col);

					y0 -= element_size.y;
					y0 -= 2.0f; // padding @cleanup
				}	

				// craft button
				{
					Vector2 size = v2(section_size.x * 0.8, 6.0);

					x0 = bottom_left_right_pane.x + (section_size.x - size.x) * 0.5;
					y0 = bottom_left_right_pane.y;
					y0 += 5.0f; // padding from bottom @cleanup

					// check for all ingredients met
					bool has_enough_for_crafting = true;
					for (int i = 0; i < get_crafting_recipe_count(selected_item_data); i++) {
						ItemAmount ingredient_amount = selected_item_data.crafting_recipe[i];
						if (ingredient_amount.amount > world->inventory_items[ingredient_amount.id].amount) {
							has_enough_for_crafting = false;
							break;
						}
					}

					Range2f btn_range = range2f_make_bottom_right(v2(x0, y0), size);
					Vector4 col = fill_col;
					if (has_enough_for_crafting && range2f_contains(btn_range, get_mouse_pos_in_world_space())) {
						col = COLOR_RED;
						world_frame.hover_consumed = true;
						// TODO - where do we put the state for the animation of the button?
						// Either just manually rip it via the App state, or some kind of hash string thing that's probably overcomplicated as shit.
						if (is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
							consume_key_just_pressed(MOUSE_BUTTON_LEFT);
						// :craft!
							workbench_en->current_crafting_item = world->selected_crafting_item;
							workbench_en->current_crafting_amount += 1;
							// remove ingredients from inventory
							for (int i = 0; i < get_crafting_recipe_count(selected_item_data); i++) {
								ItemAmount ingredient_amount = selected_item_data.crafting_recipe[i];
								world->inventory_items[ingredient_amount.id].amount -= ingredient_amount.amount;
								assert(world->inventory_items[ingredient_amount.id].amount >= 0, "removed too many items. the check above must have failed!");
							}
						}
					}
					// todo - disable button with has_enough_for_crafting
					draw_rect(v2(x0, y0), size, col);

					string txt = STR("CRAFT");
					Gfx_Text_Metrics metrics = measure_text(font, txt, font_height, v2(0.1, 0.1));
					float center_pos = bottom_left_right_pane.x + section_size.x * 0.5;
					Vector2 draw_pos = v2(center_pos, y0 + size.y * 0.5);
					draw_pos = v2_sub(draw_pos, metrics.visual_pos_min);
					draw_pos = v2_sub(draw_pos, v2_mul(metrics.visual_size, v2(0.5, 0.5)));

					draw_text(font, txt, font_height, draw_pos, v2(0.1, 0.1), COLOR_WHITE);
				}
				
			} else {
				// select item first text
			}
				y0 += section_size.y * 0.5;
				{
					string title = STR("Select Item to Craft");
					Gfx_Text_Metrics metrics = measure_text(font, title, font_height, v2(0.1, 0.1));

					Vector2 draw_pos = v2(x0 + section_size.x * 0.5, y0);
					draw_pos = v2_sub(draw_pos, metrics.visual_pos_min);
					draw_pos = v2_sub(draw_pos, v2_mul(metrics.visual_size, v2(0.5, 0.5)));

					draw_text(font, title, font_height, draw_pos, v2(0.1, 0.1), COLOR_WHITE);
				}
		}
				world_frame.hover_consumed = true;

	}
	if (world->ux_state != UX_nil && is_key_just_pressed(KEY_ESCAPE)) {
		consume_key_just_pressed(KEY_ESCAPE);
		world->ux_state = 0;
	}

	set_world_space();
	pop_z_layer();
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

		sprites[0] = (Sprite){.image = load_image_from_disk(STR("res/sprites/missingTexture.jpeg"), get_heap_allocator())};
   		sprites[SPRITE_player] = (Sprite){.image = load_image_from_disk(STR("res/sprites/player.jpeg"), get_heap_allocator())};
		sprites[SPRITE_tree0] = (Sprite){.image = load_image_from_disk(STR("res/sprites/tree00.jpeg"), get_heap_allocator())};
	    sprites[SPRITE_tree1] = (Sprite){.image = load_image_from_disk(STR("res/sprites/tree01.jpeg"), get_heap_allocator())};
		sprites[SPRITE_rock] = (Sprite){.image = load_image_from_disk(STR("res/sprites/rock0.jpeg"), get_heap_allocator())};
	    sprites[SPRITE_flower0] = (Sprite){.image = load_image_from_disk(STR("res/sprites/flower00.jpeg"), get_heap_allocator())};
	    sprites[SPRITE_flower1] = (Sprite){.image = load_image_from_disk(STR("res/sprites/flower01.jpeg"), get_heap_allocator())};
	    sprites[SPRITE_item_pine_wood] = (Sprite){.image = load_image_from_disk(STR("res/sprites/pineWoodItem.jpeg"), get_heap_allocator())};
	    sprites[SPRITE_item_cyan_pigment] = (Sprite){.image = load_image_from_disk(STR("res/sprites/cyanPigmentItem.jpeg"), get_heap_allocator())};
 	   	sprites[SPRITE_item_magenta_pigment] = (Sprite){.image = load_image_from_disk(STR("res/sprites/flower1_item.jpeg"), get_heap_allocator())};
  	 	sprites[SPRITE_item_rock] = (Sprite){.image = load_image_from_disk(STR("res/sprites/rockItem.jpeg"), get_heap_allocator())};
  	  	sprites[SPRITE_paintbrush] = (Sprite){.image = load_image_from_disk(STR("res/sprites/paintbrush.jpeg"), get_heap_allocator())};
		sprites[SPRITE_furnace] = (Sprite){.image = load_image_from_disk(STR("res/sprites/furnace.jpeg"), get_heap_allocator())};
  	  	sprites[SPRITE_workbench] = (Sprite){.image = load_image_from_disk(STR("res/sprites/workbench.jpeg"), get_heap_allocator())};
    	sprites[SPRITE_paintStation] = (Sprite){.image = load_image_from_disk(STR("res/sprites/paintStation.jpeg"), get_heap_allocator())};

	{
		for (SpriteID i = 0; i < SPRITE_MAX; i++) {
			Sprite* sprite = &sprite[i];
			assert(sprite->image, "Sprite was not setup properly");
		}
	}


    font = load_font_from_disk(STR("C:/windows/fonts/arial.ttf"), get_heap_allocator());
	assert(font, "Failed loading arial.ttf, %d", GetLastError());

	// building resource setup
	{
		// buildings[0] =
		buildings[BUILDING_furnace] = (BuildingData){ .to_build=ARCH_furnace, .icon=SPRITE_furnace};
		buildings[BUILDING_workbench] = (BuildingData){ .to_build=ARCH_workbench, .icon=SPRITE_workbench};
		buildings[BUILDING_paint_station] = (BuildingData){ .to_build=ARCH_paint_station, .icon=SPRITE_paintStation};
	}

	// item data resource setup
	{
		// do defaults
		for (ItemID i = 1; i < ITEM_MAX; i++) {
			ItemData* data = &item_data[i];
			data->craft_length = 2.0;
		}

		item_data[ITEM_rock] = (ItemData){ .pretty_name=STR("Rock"), .for_structure=ARCH_furnace, .crafting_recipe ={ {ITEM_pine_wood, 2} } };
		item_data[ITEM_pine_wood] = (ItemData){ .pretty_name=STR("Pine Wood"), .for_structure=ARCH_workbench, .crafting_recipe ={ {ITEM_pine_wood, 5}, {ITEM_rock, 1} } };
		item_data[ITEM_cyan_pigment] = (ItemData){ .pretty_name=STR("Cyan Pigment"), .for_structure=ARCH_paint_station, .crafting_recipe ={ {ITEM_cyan_pigment, 2} } };

	}


	// :init 

	// test stuff
	// TODO - @ship debug this off
	{
		world->inventory_items[ITEM_pine_wood].amount = 50;
		world->inventory_items[ITEM_cyan_pigment].amount = 50;
		world->inventory_items[ITEM_magenta_pigment].amount = 50;
		world->inventory_items[ITEM_rock].amount = 50;	

		Entity* en = entity_create();
		setup_furnace(en);	
	}

	Entity* player_en = entity_create();
	setup_player(player_en);

	const int MAX_FLOWER_ENTITY_AMOUNT = 10;
	const int MAX_TREE_ENTITY_AMOUNT = 10;
 	const int MAX_ROCK_ENTITY_AMOUNT = 10;


	for (int i = 0; i < MAX_FLOWER_ENTITY_AMOUNT; i++){
		Entity* en = entity_create();
		setup_flower(en);
		en->pos = v2(get_random_float32_in_range(-200, 200), get_random_float32_in_range(-200, 200));
		en->pos = round_v2_to_tile(en->pos);
		//en->pos.y -= tile_width * 0.5;
	}
	for (int i = 0; i < MAX_TREE_ENTITY_AMOUNT; i++){
		Entity* en = entity_create();
		setup_tree(en);
		en->pos = v2(get_random_float32_in_range(-200, 200), get_random_float32_in_range(-200, 200));
		en->pos = round_v2_to_tile(en->pos);
		//en->pos.y -= tile_width * 0.5;
	}
	for (int i = 0; i < MAX_ROCK_ENTITY_AMOUNT; i++){
		Entity* en = entity_create();
		setup_rock(en);
		en->pos = v2(get_random_float32_in_range(-200, 200), get_random_float32_in_range(-200, 200));
		en->pos = round_v2_to_tile(en->pos);
		//en->pos.y -= tile_width * 0.5;
	}
	
	float64 seconds_counter = 0.0;
	s32 frame_count = 0;

	float zoom = 5.3;
	Vector2 camera_pos = v2(0, 0);

	float64 last_time = os_get_elapsed_seconds();

	while (!window.should_close) {
		reset_temporary_storage();
		world_frame = (WorldFrame){0};
		float64 current_time = os_get_elapsed_seconds();
		delta_t = current_time - last_time;
		last_time = current_time;
		os_update(); 
		
		// :frame :update
		draw_frame.enable_z_sorting = true;

		world_frame.world_proj = m4_make_orthographic_projection(window.width * -0.5, window.width * 0.5, window.height * -0.5, window.height * 0.5, -1, 10);

		// :camera
		{
			Vector2 target_pos = player_en->pos;
			animate_v2_to_target(&camera_pos, target_pos, delta_t, 30.0f);

			world_frame.world_view = m4_make_scale(v3(1.0, 1.0, 1.0));
			world_frame.world_view = m4_mul(world_frame.world_view, m4_make_translation(v3(camera_pos.x, camera_pos.y, 0)));
			world_frame.world_view = m4_mul(world_frame.world_view, m4_make_scale(v3(1.0/zoom, 1.0/zoom, 1.0)));
		}

		set_world_space();
		push_z_layer(layer_world);
		
		Vector2 mouse_pos_world = get_mouse_pos_in_world_space();
		int mouse_tile_x = world_pos_to_tile_pos(mouse_pos_world.x);
		int mouse_tile_y = world_pos_to_tile_pos(mouse_pos_world.y); 

		// :do UI rendering
		do_ui_stuff();

		// select entity
		if(!world_frame.hover_consumed)
		{
			//draw_text(font, sprint(get_temporary_allocator(), "%f %f" , mouse_pos_world.x, mouse_pos_world.y), font_height, mouse_pos_world, v2(0.1, 0.1), COLOR_RED);

			float smallest_dist = 0.000000001;
			// :selected entity system		
			for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
				Entity* en = &world->entities[i];
				
				if(en->is_valid && (en->detroyable_world_item || en->workbench_thing)) {
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

		// :update entities
		
		for (int i = 0; i < MAX_ENTITY_COUNT; i++) {
			Entity* en = &world->entities[i];
			if(en->is_valid) {
				if (en->workbench_thing) {
					if (en->current_crafting_item) {
						float length = en->current_crafting_item;
						if (en->crafting_end_time == 0) {
							en->crafting_end_time = now() + length;
						}

						float alpha = alpha_from_end_time(en->crafting_end_time, length);
						// log("%f", alpha);

						if (has_reached_end_time(en->crafting_end_time)) {
							// craft thing
							{
								Entity* item = entity_create();
								setup_item(item, en->current_crafting_item);
								item->pos = en->pos;
							}
							en->current_crafting_amount -= 1;
							en->crafting_end_time = 0;
							assert(en->current_crafting_amount >= 0, "fuckie wuckie");
							if (en->current_crafting_amount == 0) {
								en->current_crafting_item = 0;
							}
						}
					}
				}

				// pick up item
				if(en->is_item) {
					//TODO - physics pickup

					if (fabsf(v2_dist(en->pos, player_en->pos)) < player_pickup_radius) {
						world->inventory_items[en->item_id].amount += 1;
						entity_destroy(en);
					}
				}					
			}
		}
		
		// :selection stuff
	{
			Entity* selected_en = world_frame.selected_entity;

		// :click destroy
		if (selected_en && selected_en->detroyable_world_item) {
			if(is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
				consume_key_just_pressed(MOUSE_BUTTON_LEFT);

				if(selected_en) {
					selected_en->health -= 1;
					if(selected_en->health <= 0) {

						switch (selected_en->arch) {
						case ARCH_flower: {							
							Entity* en = entity_create();
							setup_item(en, ITEM_cyan_pigment);
							en->pos = selected_en->pos;							
						}
						break;
						case ARCH_tree: {							
							Entity* en = entity_create();
							setup_item(en, ITEM_pine_wood);
							en->pos = selected_en->pos;							
						}
						break;
						case ARCH_rock: {							
							Entity* en = entity_create();
							setup_item(en, ITEM_rock);
							en->pos = selected_en->pos;							
						}
						break;
						default: {}	break;
						}

						entity_destroy(selected_en);
					}
				}
			}
		}

		// :interact
		if (selected_en && selected_en->workbench_thing) {
			if(is_key_just_pressed(MOUSE_BUTTON_LEFT)) {
				consume_key_just_pressed(MOUSE_BUTTON_LEFT);

				world->ux_state = UX_workbench;
				world->open_workbench = selected_en;
			}
		}
	}

		// :render entities
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
						
						// @volatile with entity placement
						xform         = m4_translate(xform, v3(0, tile_width * -0.5, 0));
						xform         = m4_translate(xform, v3(en->pos.x, en->pos.y, 0));
						xform         = m4_translate(xform, v3(get_sprite_size(sprite).x * -0.5, 0.0, 0));

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

				// craft progress bar thing
				if (en->current_crafting_item && en->workbench_thing) {
					float radius = 4.0; 

					{
						Matrix4 xform = m4_identity;
						xform = m4_translate(xform, v3(en->pos.x, en->pos.y + 14.0, 0));
						xform = m4_translate(xform, v3(-radius, -radius, 0));
						draw_circle_xform(xform, v2(radius*2, radius*2), COLOR_WHITE);
					}

					ItemData craft_item_data = get_item_data(en->current_crafting_item);

					float alpha = alpha_from_end_time(en->crafting_end_time, en->current_crafting_item);

					{
						Matrix4 xform = m4_identity;
						xform = m4_translate(xform, v3(en->pos.x, en->pos.y + 14.0, 0));
						xform = m4_scale(xform, v3(alpha, alpha, 1.0));
						xform = m4_translate(xform, v3(-radius, -radius, 0));
						draw_circle_xform(xform, v2(radius*2, radius*2), COLOR_BLACK);
					}
				}
			}
		}


		// input		
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