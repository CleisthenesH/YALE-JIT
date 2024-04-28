// Copyright 2023-2024 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

//#define WIDGET_DEBUG_DRAW

#include "widget.h"
#include "thread_pool.h"
#include "material.h"
#include "resource_manager.h"

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_opengl.h>
#include <allegro5/allegro_primitives.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <luajit.h>

#include <stdio.h>
#include <float.h>
#include <math.h>

// Global Variables
extern double mouse_x;
extern double mouse_y;
extern double current_timestamp;
extern double delta_timestamp;
extern ALLEGRO_EVENT current_event;
extern const ALLEGRO_TRANSFORM identity_transform;
extern void invert_transform_3D(ALLEGRO_TRANSFORM*);
extern lua_State* const lua_state;
extern ALLEGRO_FONT* debug_font;

// Zone and piece settings
static bool auto_snap;					// When making a potential move automatically snap the piece valid zones.
static bool auto_highlight;			    // When making a potential move automatically highlight valid zone.
static bool auto_transition;			// After making a move auto transition to the zones snap.
static bool auto_self_highlight;		// Highlight the zone a piece comes from but block it from calling vaild_move.

// Widget constructors
extern int material_test_new(lua_State*); // Not implemented
extern int frame_new(lua_State*);
extern int button_new(lua_State*);
extern int counter_new(lua_State*);
extern int text_entry_new(lua_State*);
extern int slider_new(lua_State*);
extern int drop_down_new(lua_State*);
extern int tile_selector_new(lua_State*);
extern int tile_new(lua_State*);
extern int meeple_new(lua_State*);

/*********************************************/
/*   Memory Layout and Pointer Arithmetic    */
/*********************************************/

// Header
// Jumptable
// Base (first entry is keyframe)
// Zone/Piece/HUD Particulars 
// General Particulars

// TODO: clean up to bit field
enum wg_type
{
    WG_ZONE,
    WG_PIECE,
    WG_HUD,
    WG_FRAME,
};

struct wg_header
{
    enum wg_type type;

    // Hierarchy
    struct wg_internal* next;
    struct wg_internal* previous;
    union {
        struct {
            struct wg_internal* head;
            struct wg_internal* tail;
        };
		struct wg_internal* parent;
    };
};

struct wg_internal
{
    struct wg_header;
    struct wg_jumptable_base* jumptable;
    struct wg_base;
};

struct wg_zone_internal
{
    struct wg_header;
    struct wg_jumptable_zone* jumptable;
    struct wg_zone;
};

struct wg_piece_internal
{
    struct wg_header;
    struct wg_jumptable_piece* jumptable;
    struct wg_piece;
};

struct wg_frame_internal
{
    struct wg_header;
    struct wg_jumptable_frame* jumptable;
    struct wg_frame;
};

struct wg_hud_internal
{
    struct wg_header;
    struct wg_jumptable_hud* jumptable;
    struct wg_hud;
};

static inline struct wg_base* wg_public(struct wg_internal* wg)
{
    return (struct wg_base*)((char*)wg + sizeof(struct wg_header) + sizeof(struct wg_jumptable_base*));
}

static inline struct wg_internal* wg_internal(struct wg_base* wg)
{
    return (struct wg_internal*)((char*)wg - sizeof(struct wg_header) - sizeof(struct wg_jumptable_base*));
}

static struct geometry* wg_geometry(struct wg_internal* wg)
{
    return (struct geometry*)wg_public(wg);
}

/*********************************************/
/*              Widgets Untility             */
/*********************************************/

static bool wg_is_branch(struct wg_internal* wg)
{
	return (wg->type == WG_ZONE || wg->type == WG_FRAME);
}

static bool wg_is_leaf(struct wg_internal* wg)
{
	return (wg->type == WG_PIECE || wg->type == WG_HUD);
}

static bool wg_is_board(struct wg_internal* wg)
{
    return (wg->type == WG_PIECE || wg->type == WG_ZONE);
}

static bool wg_is_hud(struct wg_internal* wg)
{
    return (wg->type == WG_HUD || wg->type == WG_FRAME);
}

static void wg_append(struct wg_internal* branch, struct wg_internal* leaf)
{
    if (branch->tail)
        branch->tail->next = leaf;
    else
        branch->head = leaf;

    leaf->parent = branch;
    leaf->previous = branch->tail;

    branch->tail = leaf;
}

static void wg_remove(struct wg_internal* const leaf)
{
	struct wg_internal* parent = leaf->parent;

	if (leaf->next)
		leaf->next->previous = leaf->previous;
	else if (parent && parent->tail == leaf)
		parent->tail = leaf->previous;

	if (leaf->previous)
		leaf->previous->next = leaf->next;
	else if (parent && parent->head == leaf)
		parent->head = leaf->next;
}

/*********************************************/
/*                 Geometry                  */
/*********************************************/

void geometry_default(struct geometry* const geometry)
{
    *geometry = (struct geometry)
    {
        .sx = 1,
        .sy = 1,
        .hh = 50,
        .hw = 50,
    };
}

void geometry_copy(struct geometry* const dest, const struct geometry* const src)
{
    memcpy_s(dest, sizeof(struct geometry), src, sizeof(struct geometry));
}

void geometry_blend(struct geometry* const dest, const struct geometry* const a, const struct geometry* const b, double blend)
{
    dest->x = a->x * blend + b->x * (1 - blend);
    dest->y = a->y * blend + b->y * (1 - blend);
    dest->sx = a->sx * blend + b->sx * (1 - blend);
    dest->sy = a->sy * blend + b->sy * (1 - blend);
    dest->a = a->a * blend + b->a * (1 - blend);
    dest->dx = a->dx * blend + b->dx * (1 - blend);
    dest->dy = a->dy * blend + b->dy * (1 - blend);
    dest->hh = a->hh * blend + b->hh * (1 - blend);
    dest->hw = a->hw * blend + b->hw * (1 - blend);
}

// Read a transform from the top of the stack to a pointer
void lua_getgeometry(int idx, struct geometry* const geometry)
{
    lua_getfield(lua_state, idx, "x");
    if (lua_type(lua_state, -1) == LUA_TNUMBER)
        geometry->x = lua_tonumber(lua_state, -1);

    lua_getfield(lua_state, idx - 1, "y");
    if (lua_type(lua_state, -1) == LUA_TNUMBER)
        geometry->y = lua_tonumber(lua_state, -1);

    lua_getfield(lua_state, idx - 2, "sx");
    if (lua_type(lua_state, -1) == LUA_TNUMBER)
        geometry->sx = lua_tonumber(lua_state, -1);

    lua_getfield(lua_state, idx - 3, "sy");
    if (lua_type(lua_state, -1) == LUA_TNUMBER)
        geometry->sy = lua_tonumber(lua_state, -1);

    lua_getfield(lua_state, idx - 4, "a");
    if (lua_type(lua_state, -1) == LUA_TNUMBER)
        geometry->a = lua_tonumber(lua_state, -1);

    lua_getfield(lua_state, idx - 5, "c");
    if (lua_type(lua_state, -1) == LUA_TNUMBER)
        geometry->c = lua_tonumber(lua_state, -1);

    lua_getfield(lua_state, idx - 6, "dx");
    if (lua_type(lua_state, -1) == LUA_TNUMBER)
        geometry->dx = lua_tonumber(lua_state, -1);

    lua_getfield(lua_state, idx - 7, "dy");
    if (lua_type(lua_state, -1) == LUA_TNUMBER)
        geometry->dy = lua_tonumber(lua_state, -1);

    lua_getfield(lua_state, idx - 8, "hh");
    if (lua_type(lua_state, -1) == LUA_TNUMBER)
        geometry->hh = lua_tonumber(lua_state, -1);

    lua_getfield(lua_state, idx - 9, "hw");
    if (lua_type(lua_state, -1) == LUA_TNUMBER)
        geometry->hw = lua_tonumber(lua_state, -1);

    lua_pop(lua_state, 10);
}

// Set the field of a table at idx to the keyframe memebers.
void lua_setgeometry(int idx, const struct geometry* const geometry)
{
    lua_pushnumber(lua_state, geometry->x);
    lua_setfield(lua_state, idx - 1, "x");
    lua_pushnumber(lua_state, geometry->y);
    lua_setfield(lua_state, idx - 1, "y");
    lua_pushnumber(lua_state, geometry->sx);
    lua_setfield(lua_state, idx - 1, "sx");
    lua_pushnumber(lua_state, geometry->sy);
    lua_setfield(lua_state, idx - 1, "sy");
    lua_pushnumber(lua_state, geometry->a);
    lua_setfield(lua_state, idx - 1, "a");
    lua_pushnumber(lua_state, geometry->c);
    lua_setfield(lua_state, idx - 1, "c");
    lua_pushnumber(lua_state, geometry->dx);
    lua_setfield(lua_state, idx - 1, "dx");
    lua_pushnumber(lua_state, geometry->dy);
    lua_setfield(lua_state, idx - 1, "dy");
    lua_pushnumber(lua_state, geometry->hh);
    lua_setfield(lua_state, idx - 1, "hh");
    lua_pushnumber(lua_state, geometry->hw);
    lua_setfield(lua_state, idx - 1, "hw");
}

// Removes keyframe keys from table at idx.
void lua_cleangeometry(int idx)
{
    lua_pushnil(lua_state);
    lua_setfield(lua_state, idx - 1, "t");
    lua_pushnil(lua_state);
    lua_setfield(lua_state, idx - 1, "x");
    lua_pushnil(lua_state);
    lua_setfield(lua_state, idx - 1, "y");
    lua_pushnil(lua_state);
    lua_setfield(lua_state, idx - 1, "sx");
    lua_pushnil(lua_state);
    lua_setfield(lua_state, idx - 1, "sy");
    lua_pushnil(lua_state);
    lua_setfield(lua_state, idx - 1, "a");
    lua_pushnil(lua_state);
    lua_setfield(lua_state, idx - 1, "c");
    lua_pushnil(lua_state);
    lua_setfield(lua_state, idx - 1, "dx");
    lua_pushnil(lua_state);
    lua_setfield(lua_state, idx - 1, "dy");
    lua_pushnil(lua_state);
    lua_setfield(lua_state, idx - 1, "hh");
    lua_pushnil(lua_state);
    lua_setfield(lua_state, idx - 1, "hw");
}

/*********************************************/
/*                  Bézier                   */
/*********************************************/

// The widget wg is controled through a standard 
//  Cubic Bézier Curve with the following points:
// P0: &wg_keyframe(wg)
// P1: wg->ctrl1
// P2: wg->ctrl2
// P3: wg->dest

static void wg_bezier_set(struct wg_internal* wg, struct geometry* geometry)
{
	geometry_copy(wg_geometry(wg), geometry);
	geometry_copy(&wg->ctrl1, geometry);
	geometry_copy(&wg->ctrl2, geometry);
	geometry_copy(&wg->dest, geometry);

    wg->t = current_timestamp;
}

static void wg_bezier_update(struct wg_internal* wg)
{
    const double dt = wg->t - current_timestamp;

    if (dt < 0.0)
        return;

    if (delta_timestamp >= dt)
    {
        wg_bezier_set(wg, &wg->dest);
        return;
    }

    // Warning: Standard numeric stability concerns.
    double const blend = delta_timestamp / (wg->t - current_timestamp);

    struct geometry* const bezier[4] = {
        wg_geometry(wg),
        &wg->ctrl1,
        &wg->ctrl2,
        &wg->dest
    };

    geometry_blend(bezier[0], bezier[1], bezier[0], blend);
    geometry_blend(bezier[1], bezier[2], bezier[1], blend);
    geometry_blend(bezier[2], bezier[3], bezier[2], blend);

    geometry_blend(bezier[0], bezier[1], bezier[0], blend);
    geometry_blend(bezier[1], bezier[2], bezier[1], blend);
    
    geometry_blend(bezier[0], bezier[1], bezier[0], blend);
}

static void wg_bezier_interupt(struct wg_internal* wg)
{
    geometry_copy(&wg->ctrl1, wg_geometry(wg));
    geometry_copy(&wg->ctrl2, wg_geometry(wg));
    geometry_copy(&wg->dest, wg_geometry(wg));

    wg->t = current_timestamp;
}

static void wg_bezier_default(struct wg_internal* wg)
{
	geometry_default(wg_geometry(wg));
	geometry_default(&wg->ctrl1);
	geometry_default(&wg->ctrl2);
	geometry_default(&wg->dest);
}

/*********************************************/
/*                  Roots                    */
/*********************************************/

static struct root{
    struct wg_internal* head;
	struct wg_internal* tail;
	struct wg_internal* default_branch;
} *root_board, *root_hud;

static void lua_pushroot(lua_State* L, struct wg_internal* wg)
{
    if (wg_is_board(wg))
        lua_getglobal(L, "board");
    else
        lua_getglobal(L, "hud");
}

static void root_append(struct root* root, struct wg_internal* branch)
{
    if (root->tail)
        root->tail->next = branch;
    else
        root->head = branch;

    branch->previous = root->tail;

    root->tail = branch;
}

static int root_index(lua_State* L)
{
    struct root* root = luaL_checkudata(L, -2, "root_mt");
    const char* key = lua_tostring(L, -1);

    if (strcmp(key, "branches") == 0)
    {
        lua_getfenv(L, -2);
        lua_getfield(L, -1, "branches");
        return 1;
    }   
    else if (strcmp(key, "leaves") == 0)
    {
        lua_getfenv(L, -2);
        lua_getfield(L, -1, "leaves");
        return 1;
    }

    if(root == root_board)
        if (strcmp(key, "tile") == 0)
        {
            lua_pushcfunction(L, tile_new);
            return 1;
        }

    if(root == root_hud)
        if (strcmp(key, "frame") == 0)
        {
            lua_pushcfunction(L, frame_new);
            return 1;
        }

    /*
        else if (strcmp(key, "meeple") == 0)
        {
            lua_pushcfunction(L, tile_new);
            return 1;
        }
        */



    lua_getfenv(L, -2);
    lua_replace(L, -3);

    lua_gettable(L, -2);

    return 1;
}

static int root_newindex(lua_State* L)
{
    struct root* root = luaL_checkudata(L, -3, "root_mt");
    const char* key = lua_tostring(L, -2);

    lua_getfenv(L, -3);
    lua_replace(L, -4);

    lua_settable(L, -3);

    return 0;
}

static void init_roots()
{
    root_board = lua_newuserdata(lua_state, sizeof(struct root));
    root_hud = lua_newuserdata(lua_state, sizeof(struct root));

    *root_board = (struct root){0};
    *root_board = (struct root){0};

    // Make the and set metatables
    luaL_newmetatable(lua_state, "root_mt");

    lua_pushcfunction(lua_state, root_index);
    lua_setfield(lua_state, -2, "__index");

    lua_pushcfunction(lua_state, root_newindex);
    lua_setfield(lua_state, -2, "__newindex");
    
    lua_pushvalue(lua_state, -1);
    lua_setmetatable(lua_state, -3);
    lua_setmetatable(lua_state, -3);

    // Make and set fenv
    for (int i = 0; i < 2; i++)
    {
        lua_newtable(lua_state);

        lua_newtable(lua_state);
        lua_setfield(lua_state, -2, "branches");

        lua_newtable(lua_state);
        lua_setfield(lua_state, -2, "leaves");
    }

    lua_setfenv(lua_state, -3);
    lua_setfenv(lua_state, -3);

    // Set globals
    lua_setglobal(lua_state, "hud");
    lua_setglobal(lua_state, "board");
}

static void lua_pushwidget(lua_State* L, struct wg_internal* wg)
{
    if (wg_is_hud(wg))
        lua_getglobal(L, "hud");
    else
        lua_getglobal(L, "board");

    lua_getfenv(L, -1);

    if (wg_is_leaf(wg))
        lua_getfield(L, -1, "leaves");
    else
        lua_getfield(L, -1, "branches");

    lua_pushlightuserdata(L, wg);
    lua_gettable(L, -2);
    lua_replace(L, -4);
    lua_pop(L, 2);
}

/*********************************************/
/*           Widget Engine State             */
/*********************************************/

static enum {
    ENGINE_STATE_IDLE,                  // Idle state.
    ENGINE_STATE_HOVER,                 // A widget is being hovered but the mouse is up.
    ENGINE_STATE_PRE_DRAG_THRESHOLD,    // Inbetween mouse down and callback (checking if click or drag).
    ENGINE_STATE_POST_DRAG_THRESHOLD,   // The drag threshold has been reached but the widget can't be dragged.
    ENGINE_STATE_DRAG,                  // A widget is getting dragged and is located on the mouse.
    ENGINE_STATE_TO_DRAG,               // A widget is getting dragged and is moving towards the mouse.
    ENGINE_STATE_TO_SNAP,               // A widget is getting dragged and is mocing towards the snap.
    ENGINE_STATE_SNAP,                  // A widget is getting dragged and is located on the snap.
    ENGINE_STATE_EMPTY_DRAG,            // Drag but without a widget underneath. Used to control camera.
    ENGINE_STATE_TABBED_OUT             // The engine has been tabbed out from.
} widget_engine_state;

static double transition_timestamp;

static const char* engine_state_str[] = {
       "Idle",
       "Hover",
       "Click (Pre-Threshold)",
       "Click (Post-Threshold)",
       "Drag",
       "To Drag",
       "To Snap",
       "Snap",
       "Empty Drag",
       "Tabbed Out"
};

// Whether the current_hover should be rendered ontop of other widgets.
static bool hover_on_top()
{
    return widget_engine_state == ENGINE_STATE_DRAG ||
        widget_engine_state == ENGINE_STATE_SNAP ||
        widget_engine_state == ENGINE_STATE_TO_SNAP ||
        widget_engine_state == ENGINE_STATE_TO_DRAG;
}

// Click, Drag, and Drop pointers
static struct wg_internal* last_click;
static struct wg_internal* current_hover;
static struct wg_internal* current_drop;

/*********************************************/
/*                   Camera                  */
/*********************************************/

static struct wg_internal camera;

static void camera_build_transform(const struct geometry* const geometry, ALLEGRO_TRANSFORM* const trans)
{
    al_build_transform(trans,
        geometry->x, geometry->y,
        geometry->sx, geometry->sy,
        geometry->a);

    al_translate_transform(trans, geometry->dx, geometry->dy);

    ALLEGRO_TRANSFORM buffer;

    const double blend_x = camera.x * geometry->c;
    const double blend_y = camera.y * geometry->c;
    const double blend_sx = camera.sx * geometry->c + (1 - geometry->c);
    const double blend_sy = camera.sy * geometry->c + (1 - geometry->c);
    const double blend_a = camera.a * geometry->c;

    al_build_transform(&buffer,
        blend_x, blend_y,
        blend_sx, blend_sy,
        blend_a);

    al_compose_transform(trans, &buffer);
}

// Push camera
static int camera_push(lua_State* L)
{
    luaL_checktype(L, -1, LUA_TTABLE);

    struct geometry geometry;
    geometry_copy(&geometry, &camera.dest);
    lua_getgeometry(-1, &geometry);
    geometry_copy(&camera.dest, & geometry);

    lua_getfield(L, -1, "t");

    if (lua_type(lua_state, -1) == LUA_TNUMBER)
        camera.t = lua_tonumber(lua_state, -1);
    else
        camera.t = current_timestamp;

    return 0;
}

// Set camera
static int camera_set(lua_State* L)
{
    luaL_checktype(L, -1, LUA_TTABLE);

    struct geometry geometry;
    geometry_copy(&geometry, &camera.dest);
    lua_getgeometry(-1, &geometry);
    wg_bezier_set(&camera, &geometry);

    return 0;
}

static void camera_init()
{
    geometry_default(wg_geometry(&camera));
    wg_bezier_default(&camera);
    lua_pushcfunction(lua_state, camera_push);
    lua_setglobal(lua_state, "camera_push");

    lua_pushcfunction(lua_state, camera_set);
    lua_setglobal(lua_state, "camera_set");
}

/*********************************************/
/*                  Shaders                  */
/*********************************************/

// Pick (Offscreen drawing) Variables
static ALLEGRO_SHADER* offscreen_shader;
static ALLEGRO_BITMAP* offscreen_bitmap;

static ALLEGRO_SHADER* onscreen_shader;

static void offscreen_shader_init()
{
    // Build the offscreen shader and bitmap
    offscreen_shader = al_create_shader(ALLEGRO_SHADER_GLSL);

    if (!al_attach_shader_source_file(offscreen_shader, ALLEGRO_VERTEX_SHADER, "shaders/offscreen.vert"))
    {
        fprintf(stderr, "Failed to attach vertex shader.\n%s\n", al_get_shader_log(offscreen_shader));
        return;
    }

    if (!al_attach_shader_source_file(offscreen_shader, ALLEGRO_PIXEL_SHADER, "shaders/offscreen.frag"))
    {
        fprintf(stderr, "Failed to attach pixel shader.\n%s\n", al_get_shader_log(offscreen_shader));
        return;
    }

    if (!al_build_shader(offscreen_shader))
    {
        fprintf(stderr, "Failed to build shader.\n%s\n", al_get_shader_log(offscreen_shader));
        return;
    }

    offscreen_bitmap = al_create_bitmap(
        al_get_bitmap_width(al_get_target_bitmap()),
        al_get_bitmap_width(al_get_target_bitmap()));
}

static void onscreen_shader_init()
{
    onscreen_shader = al_create_shader(ALLEGRO_SHADER_GLSL);

    if (!al_attach_shader_source_file(onscreen_shader, ALLEGRO_VERTEX_SHADER, "shaders/onscreen.vert"))
    {
        fprintf(stderr, "Failed to attach main renderer vertex shader.\n%s\n", al_get_shader_log(onscreen_shader));
        return;
    }

    if (!al_attach_shader_source_file(onscreen_shader, ALLEGRO_PIXEL_SHADER, "shaders/onscreen.frag"))
    {
        fprintf(stderr, "Failed to attach main renderer pixel shader.\n%s\n", al_get_shader_log(onscreen_shader));
        return;
    }

    if (!al_build_shader(onscreen_shader))
    {
        fprintf(stderr, "Failed to build main renderer shader.\n%s\n", al_get_shader_log(onscreen_shader));
        return;
    }

    ALLEGRO_DISPLAY* const display = al_get_current_display();
    const float dimensions[2] = { al_get_display_width(display),al_get_display_height(display) };

    al_use_shader(onscreen_shader);
    al_set_shader_float_vector("display_dimensions", 2, dimensions, 1);
    al_use_shader(NULL);

    return;
}

void widget_interface_shader_predraw()
{
    al_use_shader(onscreen_shader);
    glDisable(GL_STENCIL_TEST);
    al_set_shader_float("current_timestamp", current_timestamp);

    wg_bezier_update(&camera);
}

static void mask_widget(struct wg_internal* wg, size_t* picker_index)
{
    if (hover_on_top() && wg == current_hover)
    {
        picker_index++;
        return;
    }

    float color_buffer[3];
    size_t pick_buffer = (*picker_index)++;

    for (size_t i = 0; i < 3; i++)
    {
        color_buffer[i] = ((float)(pick_buffer % 200)) / 200;
        pick_buffer /= 200;
    }

    al_set_shader_float_vector("picker_color", 3, color_buffer, 1);
    ALLEGRO_TRANSFORM buffer;
    camera_build_transform((struct geometry* const)wg_geometry(wg), (ALLEGRO_TRANSFORM* const)&buffer);
    al_use_transform(&buffer);
    wg->jumptable->mask(wg_public(wg));
}

// Handle picking mouse inputs using off screen drawing.
static inline struct wg_internal* pick(int x, int y)
{
    ALLEGRO_BITMAP* original_bitmap = al_get_target_bitmap();

    al_set_target_bitmap(offscreen_bitmap);
    al_set_clipping_rectangle(x - 1, y - 1, 3, 3);
    glDisable(GL_STENCIL_TEST);

    al_clear_to_color(al_map_rgba(0, 0, 0, 0));

    al_use_shader(offscreen_shader);

    size_t picker_index = 1;

    const bool hide_hover = hover_on_top();

    struct wg_internal* zone = (struct wg_internal*)root_board->head;

    for (struct wg_internal* zone = root_board->head; zone; zone = zone->next)
        mask_widget(zone, &picker_index);

    for (struct wg_internal* zone = root_board->head; zone; zone = zone->next)
        for (struct wg_internal* piece = zone->head; piece; piece = piece->next)
            mask_widget(piece, &picker_index);

    for (struct wg_internal* frame = root_hud->head; frame; frame = frame->next)
    {
        mask_widget(frame, &picker_index);

        for (struct wg_internal* hud = frame->head; hud; hud = hud->next)
            mask_widget(hud, &picker_index);
    }

    al_set_target_bitmap(original_bitmap);

    float color_buffer[3];

    al_unmap_rgb_f(al_get_pixel(offscreen_bitmap, x, y),
        color_buffer, color_buffer + 1, color_buffer + 2);

    size_t index = round(200 * color_buffer[0]) +
        200 * round(200 * color_buffer[1]) +
        40000 * round(200 * color_buffer[2]);

    if (index == 0)
        return NULL;

    for (struct wg_internal* zone = root_board->head; zone; zone = zone->next)
        if (--index == 0)
            return zone;

    for (struct wg_internal* zone = root_board->head; zone; zone = zone->next)
        for (struct wg_internal* piece = zone->head; piece; piece = piece->next)
            if (--index == 0)
                return piece;

    for (struct wg_internal* frame = root_hud->head; frame; frame = frame->next)
    {
        if (--index == 0)
            return frame;

        for (struct wg_internal* hud = frame->head; hud; hud = hud->next)
            if (--index == 0)
                return hud;
    }

    return NULL;
}

/*********************************************/
/*          Zone and Piece Methods           */
/*********************************************/

static struct wg_piece_internal* moving_piece;
static struct wg_zone_internal* originating_zone;

static void zone_and_piece_init()
{
    auto_highlight = true;
    auto_snap = true;
    auto_transition = true;
    auto_self_highlight = true;

    originating_zone = NULL;
}

static void call_moves(struct wg_piece_internal* piece)
{
    lua_getglobal(lua_state, "board");
    lua_getfenv(lua_state, -1);
    lua_getfield(lua_state, -1, "moves");

    lua_replace(lua_state, -3);
    lua_pop(lua_state, 1);

    if (!lua_isfunction(lua_state, -1))
    {
        lua_pop(lua_state, 1);
        return;
    }

    lua_pushwidget(lua_state, piece);
    lua_pushwidget(lua_state, piece->parent);

    if (auto_self_highlight)
    {
        originating_zone = (struct wg_zone_internal*)piece->parent;

        originating_zone->valid_move = true;

        if (auto_highlight)
        {
            originating_zone->highlighted = true;

            if (originating_zone->jumptable->highlight_start)
                originating_zone->jumptable->highlight_start((struct wg_zone* const)wg_public((struct wg_internal*)originating_zone));
        }

        if (auto_snap)
            originating_zone->snappable = true;
    }

    lua_call(lua_state, 2, 1);

    if (!lua_istable(lua_state, -1))
    {
        lua_pop(lua_state, 1);
        return;
    }

    lua_pushnil(lua_state);

    while (lua_next(lua_state, -2))
    {
        if (!lua_isuserdata(lua_state, -1))
        {
            lua_pop(lua_state, 1);
            continue;
        }

        struct wg_zone_internal* zone = (struct wg_zone_internal*)luaL_checkudata(lua_state, -1, "widget_mt");

        if (zone->type != WG_ZONE)
        {
            lua_pop(lua_state, 1);
            continue;
        }

        zone->valid_move = true;

        if (auto_highlight)
        {
            zone->highlighted = true;

            if (zone->jumptable->highlight_start)
                zone->jumptable->highlight_start((struct wg_zone* const)wg_public((struct wg_internal*)zone));
        }

        if (auto_snap)
            zone->snappable = true;

        lua_pop(lua_state, 1);
    }

    lua_pop(lua_state, 1);
}

static void call_valid_move(struct wg_zone_internal* const zone, struct wg_piece_internal* const piece, bool valid)
{
	if (valid && auto_self_highlight && zone == originating_zone)
		return;

	lua_getglobal(lua_state, "board");
	lua_getfenv(lua_state, -1);

    if(valid)
	    lua_getfield(lua_state, -1, "valid_move");
    else
	    lua_getfield(lua_state, -1, "invalid_move");

	lua_replace(lua_state, -3);
	lua_pop(lua_state, 1);

    if (!lua_isfunction(lua_state, -1))
    {
        lua_pop(lua_state, 1);
        return;
    }
    
    lua_pushwidget(lua_state, piece);
    lua_pushwidget(lua_state, zone);

    lua_call(lua_state, 2, 0);
    lua_pop(lua_state, 1);
}

static void idle_zones()
{
    struct wg_zone_internal* zone = root_board->head;

    while (zone)
    {
        zone->valid_move = false;
        zone->nominated = false;

        if (auto_snap)
            zone->snappable = false;

        if (zone->jumptable->highlight_end)
            zone->jumptable->highlight_end((struct wg_zone* const)wg_public((struct wg_internal*)zone));

        zone->highlighted = false;

        zone = zone->next;
    }
}

/*********************************************/
/*                 Callbacks                 */
/*********************************************/

static void call_lua(struct wg_internal* const wg, const char* key, struct wg_internal* const obj)
{
    lua_pushwidget(lua_state, wg);

    // In the end this might be removable, keeping for now.
    if (lua_isnil(lua_state, -1))
    {
        lua_pop(lua_state, 1);
        return;
    }

    lua_getfenv(lua_state, -1);
    lua_getfield(lua_state, -1, key);

    if (lua_isnil(lua_state, -1))
    {
        lua_pop(lua_state, 3);
        return;
    }

    lua_pushvalue(lua_state, -3);

    if (obj)
    {
        lua_pushwidget(lua_state, obj);

        lua_pcall(lua_state, 2, 0, 0);
    }
    else
    {
        lua_pcall(lua_state, 1, 0, 0);
    }

    lua_pop(lua_state, 2);
}

void call_left_click(struct wg_internal* wg)
{
    if (wg->jumptable->left_click)
        wg->jumptable->left_click(wg_public(wg));

    call_lua(wg, "left_click", NULL);
}

void call_left_held(struct wg_internal* wg)
{
    if (wg->jumptable->left_held)
        wg->jumptable->left_held(wg_public(wg));

    call_lua(wg, "left_held", NULL);
}

void call_left_release(struct wg_internal* wg)
{
    if (wg->jumptable->left_release)
        wg->jumptable->left_release(wg_public(wg));

    call_lua(wg, "left_release", NULL);
}

void call_right_click(struct wg_internal* wg)
{
    if (wg->jumptable->right_click)
        wg->jumptable->right_click(wg_public(wg));

    call_lua(wg, "right_click", NULL);
}

void call_hover_start(struct wg_internal* wg)
{
    if (wg->type == WG_PIECE)
        call_moves(wg);
    else if (wg->type == WG_HUD)
    {
        struct wg_hud_internal* hud = (struct wg_hud_internal*) wg;

        if (hud->hud_state == HUD_IDLE)
            hud->hud_state = HUD_HOVER;
    }

    if (wg->jumptable->hover_start)
        wg->jumptable->hover_start(wg_public(wg));

    call_lua(wg, "hover_start", NULL);
}

void call_hover_end(struct wg_internal* wg)
{
    if (wg->type == WG_PIECE)
        idle_zones(wg);
    else if (wg->type == WG_HUD)
    {
        struct wg_hud_internal* hud = (struct wg_hud_internal*)wg;

        if (hud->hud_state == HUD_HOVER)
            hud->hud_state = HUD_IDLE;
    }

    if (wg->jumptable->hover_end)
        wg->jumptable->hover_end(wg_public(wg));

    call_lua(wg, "hover_end", NULL);
}

void call_drop_start(struct wg_internal* wg, struct wg_internal* wg2)
{
    if (wg->type == WG_ZONE && wg2->type == WG_PIECE)
        ((struct wg_zone_internal* const)wg)->nominated = true;

    if (wg->type == WG_PIECE && wg2->type == WG_PIECE)
        ((struct wg_zone_internal* const)wg->parent)->nominated = true;

    if (wg->jumptable->drop_start)
        wg->jumptable->drop_start(wg_public(wg), wg_public(wg2));

    call_lua(wg, "drop_start", wg2);
}

void call_drop_end(struct wg_internal* wg, struct wg_internal* wg2)
{
    if (wg->type == WG_ZONE && wg2->type == WG_PIECE)
        ((struct wg_zone_internal* const)wg)->nominated = false;

    if (wg->type == WG_PIECE && wg2->type == WG_PIECE)
        ((struct wg_zone_internal* const)wg->parent)->nominated = false;

    if (wg->jumptable->drop_end)
        wg->jumptable->drop_end(wg_public(wg), wg_public(wg2));

    call_lua(wg, "drop_end", wg2);
}

void call_drag_start(struct wg_internal* wg)
{
    call_lua(wg, "drag_start", NULL);

    if (wg->jumptable->drag_start)
        wg->jumptable->drag_start(wg_public(wg));
}

void call_drag_end_drop(struct wg_internal* wg, struct wg_internal* wg2)
{
    if (wg2->type != WG_PIECE)
    {
        if (wg->jumptable->drag_end_drop)
            wg->jumptable->drag_end_drop(wg_public(wg), wg_public(wg2));

        call_lua(wg, "drag_end_drop", wg2);
        return;
    }

    struct wg_piece_internal* piece = (struct wg_piece_internal*)wg2;
    struct wg_zone_internal* zone = NULL;

    if (wg->type == WG_ZONE)
        zone = wg;
    else if (wg->type == WG_PIECE)
        zone = wg->parent;

    if (!zone)
    {
        if (wg->jumptable->drag_end_drop)
            wg->jumptable->drag_end_drop(wg_public(wg), wg_public(wg2));

        call_lua(wg, "drag_end_drop", wg2);
        return;
    }

    if (!zone->valid_move)
    {
        if (wg->jumptable->drag_end_drop)
            wg->jumptable->drag_end_drop(wg_public(wg), wg_public(wg2));

        call_valid_move(zone, piece, false);
        call_lua(wg, "drag_end_drop", wg2);

        return;
    }

    call_valid_move(zone, piece, true);

    wg_remove(piece);
    wg_append((struct wg_internal*)zone, (struct wg_internal*) piece);

    struct geometry geometry;
    geometry_copy(&geometry, &zone->dest);

    geometry.hh = piece->dest.hh;
    geometry.hw = piece->dest.hw;

    geometry_copy(&piece->dest, &geometry);
    piece->t = current_timestamp + 0.1;

    if (wg->jumptable->drag_end_drop)
        wg->jumptable->drag_end_drop(wg_public(wg), wg_public(wg2));

    call_lua(wg, "drag_end_drop", wg2);
}

void call_drag_end_no_drop(struct wg_internal* wg)
{
    if (wg->jumptable->drag_end_no_drop)
        wg->jumptable->drag_end_no_drop(wg_public(wg));

    call_lua(wg, "drag_end_no_drop", NULL);
}

void call_click_off(struct wg_internal* wg)
{
    if (wg->jumptable->click_off)
        wg->jumptable->click_off(wg_public(wg));

    call_lua(wg, "click_off", NULL);
}

/*********************************************/
/*      General Widget Engine Methods        */
/*********************************************/

// Drag and Snap Variables
static struct geometry drag_release;
static double drag_offset_x, drag_offset_y; // coordinates system match current_hover (screen or world)
static double snap_offset_x, snap_offset_y; // coordinates system match snap (screen or world)
static const double snap_speed = 1000; // px per sec
static const double drag_threshold = 0.2;

// Prevents current_hover, last_click, and current_drag becoming stale
static void prevent_stale_pointers(struct wg_internal* const ptr)
{
    // WARNING: this function is called in widget gc
    //  at this location the weak references in widgets have been collected
    //  hence I have disabled the callbacks, because they are currently intertwined with call_lua
    if (current_hover == ptr)
    {
        //call(ptr, hover_end);

        widget_engine_state = ENGINE_STATE_IDLE;
        current_hover = NULL;
    }

    if (last_click == ptr)
    {
        //call(ptr, click_off)

        last_click = NULL;
    }

    if (current_drop == ptr)
    {
        //call(ptr, drag_end_no_drop)

        widget_engine_state = ENGINE_STATE_IDLE;
        current_drop = NULL;
    }
}

// Draws the given widget.
static void draw_widget(const struct wg_internal* const wg)
{
    //al_set_shader_float("variation", internal->variation);

    // You don't actually have to send this everytime, should track a count of materials that need it.
    if (wg->hw != 0 && wg->hh != 0)
    {
        const float dimensions[2] = { 1.0 / wg->hw, 1.0 / wg->hh };
        al_set_shader_float_vector("object_scale", 2, dimensions, 1);
    }

    ALLEGRO_TRANSFORM buffer;
    camera_build_transform((struct geometry* const) wg_geometry(wg), (ALLEGRO_TRANSFORM* const ) & buffer);
    al_use_transform(&buffer);

    material_apply(NULL);
    glDisable(GL_STENCIL_TEST);

    wg->jumptable->draw((const struct wg_base* const) wg_public(wg));

#ifdef WIDGET_DEBUG_DRAW
    al_draw_textf(debug_font, al_map_rgb_f(0, 1, 0), 10, 10, ALLEGRO_ALIGN_LEFT,
        "Self: %p, Prev: %p, Next: %p",
        wg, wg->previous, wg->next);
    material_apply(NULL);
    al_draw_rectangle(-wg->half_width, -wg->half_height, wg->half_width, wg->half_height, al_map_rgb_f(0, 1, 0), 1);
#endif
}

// Move the current_hover towards the drag location
static inline void towards_drag()
{
    struct geometry geometry = drag_release;
    geometry.dx = mouse_x - drag_offset_x;
    geometry.dy = mouse_y - drag_offset_y;

    geometry_copy(&current_hover->dest, &drag_release);
    current_hover->t = current_timestamp + 0.1;
}

// Updates and calls any callbacks for the last_click, current_hover, current_drop pointers
static inline void update_drag_pointers()
{
    // What pointer the picker returns depends on if something is being dragged.
    // If nothing is being dragged the pointer is what's being hovered.
    // If something is being dragged the pointer is what's under the dragged widget.
    //  (The widget under the drag is called the drop).
    struct wg_internal* const new_pointer = pick(mouse_x, mouse_y);

    if (current_hover != new_pointer && (
        widget_engine_state == ENGINE_STATE_IDLE ||
        widget_engine_state == ENGINE_STATE_HOVER))
    {
        if (current_hover)
            call_hover_end(current_hover);

        if (new_pointer)
        {
            call_hover_start(new_pointer);

            widget_engine_state = ENGINE_STATE_HOVER;
        }
        else
            widget_engine_state = ENGINE_STATE_IDLE;

        current_hover = new_pointer;

        return;
    }

    if (current_drop != new_pointer && (
        widget_engine_state == ENGINE_STATE_DRAG ||
        widget_engine_state == ENGINE_STATE_SNAP ||
        widget_engine_state == ENGINE_STATE_TO_DRAG ||
        widget_engine_state == ENGINE_STATE_TO_SNAP))
    {
        if (current_drop)
        {
            call_drop_end(current_drop, current_hover);
        }

        if (new_pointer)
        {
            call_drop_start(new_pointer, current_hover);

			if (new_pointer->snappable)
			{
                struct geometry snap_target;    

                geometry_copy(&snap_target, &new_pointer->dest);

				snap_target.dx += snap_offset_x;
				snap_target.dy += snap_offset_y;
                snap_target.hh = current_hover->dest.hh;
                snap_target.hw = current_hover->dest.hw;

                geometry_copy(&current_hover->dest, &snap_target);
                current_hover->t = current_timestamp + 0.1;

				widget_engine_state = ENGINE_STATE_TO_SNAP;
			}
			else
			{
				towards_drag();
				widget_engine_state = ENGINE_STATE_TO_DRAG;
			}
        }
        else
        {
            if (current_drop && current_drop->snappable)
            {
                towards_drag();
                widget_engine_state = ENGINE_STATE_TO_DRAG;
            }
            else
                widget_engine_state = ENGINE_STATE_DRAG;
        }

        current_drop = new_pointer;

        return;
    }
}

// Convert a screen position to the cordinate used when drawng
void widget_screen_to_local(const struct wg_base* const wg, double* x, double* y)
{
    ALLEGRO_TRANSFORM transform;

    // The allegro uses float but standards have moved forward to doubles.
    // This is the easist solution.
    float _x = *x;
    float _y = *y;

    camera_build_transform(wg_geometry(wg_internal(wg)), &transform);

    // WARNING: the inbuilt invert only works for 2D transforms
    al_invert_transform(&transform);
    //invert_transform_3D(&transform);

    al_transform_coordinates(&transform, &_x, &_y);

    *x = _x;
    *y = _y;
}

struct wg_base* check_widget_lua(int idx, const struct wg_jumptable_base* const jumptable)
{
    if (lua_type(lua_state, idx) != LUA_TUSERDATA)
        return NULL;

    struct wg_internal* wg = (struct wg_internal*) luaL_checkudata(lua_state, idx, "widget_mt");

    if (!wg || wg->jumptable != jumptable)
        return NULL;

    return wg_public(wg);
}

// Change widget_engine_state __like__ ALLEGRO_EVENT_MOUSE_BUTTON_UP occurred.
static void process_mouse_up(unsigned int button, bool allow_drag_end_drop)
{
    if (widget_engine_state == ENGINE_STATE_EMPTY_DRAG)
    {
        widget_engine_state = ENGINE_STATE_IDLE;
        return;
    }

    // Split up just in case empty_drag and current_hover desync
    if (!current_hover)
        return;

    switch (widget_engine_state)
    {
    case ENGINE_STATE_PRE_DRAG_THRESHOLD:
        if (button == 1)
        {
            call_left_click(current_hover);

            if (current_hover)
                call_left_release(current_hover);

            // This means hover_start can be called twice without a hover_end
            // Not commited to it (should filter for existance of drag_start?)
            if (current_hover)
                call_hover_start(current_hover);
            break;
        }
        else if(button == 2)
            call_right_click(current_hover);

    case ENGINE_STATE_POST_DRAG_THRESHOLD:
        if (button == 1)
        {
            call_left_release(current_hover);

            // This means hover_start can be called twice without a hover_end
            // Not commited to it
            if (current_hover)
                call_hover_start(current_hover);
        }

        break;

    case ENGINE_STATE_DRAG:
    case ENGINE_STATE_SNAP:
    case ENGINE_STATE_TO_SNAP:
    case ENGINE_STATE_TO_DRAG:
        geometry_copy(&current_hover->dest, &drag_release);
        current_hover->t = current_timestamp + 0.1;

        if (current_drop && allow_drag_end_drop)
        {
            call_drag_end_drop(current_drop,current_hover);
            if (current_hover)
                call_drag_end_no_drop(current_hover);
        }
        else
            call_drag_end_no_drop(current_hover);

        current_drop = NULL;
        break;
    }

    widget_engine_state = current_hover ? ENGINE_STATE_HOVER : ENGINE_STATE_IDLE;
}

/*********************************************/
/*            Big Four Callbacks             */
/*********************************************/

// Draw the widgets in queue order.
void widget_engine_draw()
{    
    for (struct wg_internal* zone = root_board->head; zone; zone = zone->next)
        draw_widget(zone);

    for (struct wg_internal* zone = root_board->head; zone; zone = zone->next)
        for (struct wg_internal* piece = zone->head; piece; piece = piece->next)
            draw_widget(piece);

    for (struct wg_internal* frame = root_hud->head; frame; frame = frame->next)
    {
        draw_widget(frame);

        for (struct wg_internal* hud = frame->head; hud; hud = hud->next)
            draw_widget(hud);
    }

    if (widget_engine_state == ENGINE_STATE_TABBED_OUT)
    {
        al_use_transform(&identity_transform);

        ALLEGRO_DISPLAY* display = al_get_current_display();

        al_draw_filled_rectangle(0, 0,
            al_get_display_width(display), al_get_display_height(display),
            al_map_rgba_f(0, 0, 0, 0.5));
    }

#ifdef WIDGET_DEBUG_DRAW
    al_use_shader(NULL);
    glDisable(GL_STENCIL_TEST);
    al_use_transform(&identity_transform);

    al_draw_textf(debug_font, al_map_rgb_f(0, 1, 0), 10, 10, ALLEGRO_ALIGN_LEFT,
        "State: %s", engine_state_str[widget_engine_state]);
    al_draw_textf(debug_font, al_map_rgb_f(0, 1, 0), 10, 40, ALLEGRO_ALIGN_LEFT,
        "Hover: %p", current_hover);
    al_draw_textf(debug_font, al_map_rgb_f(0, 1, 0), 10, 70, ALLEGRO_ALIGN_LEFT,
        "Drop: %p", current_drop);
    al_draw_textf(debug_font, al_map_rgb_f(0, 1, 0), 10, 100, ALLEGRO_ALIGN_LEFT,
        "Drag Release: (%f, %f) %f (%f, %f)", drag_release.x, drag_release.y,
        drag_release.t, drag_release.dx, drag_release.dy);
#endif
}

// Update the widget engine, done while widget tweeners are processing.
void widget_engine_update()
{
    if (widget_engine_state == ENGINE_STATE_TABBED_OUT)
        return;

    update_drag_pointers();

    if (current_timestamp > transition_timestamp)
        switch (widget_engine_state)
        {
        case ENGINE_STATE_PRE_DRAG_THRESHOLD:
            if (current_hover->draggable)
            {
                call_drag_start(current_hover);

                towards_drag();
                widget_engine_state = ENGINE_STATE_TO_DRAG;
            }
            else
            {
                call_left_click(current_hover);

                widget_engine_state = ENGINE_STATE_POST_DRAG_THRESHOLD;
            }
            break;
        case ENGINE_STATE_TO_DRAG:
            widget_engine_state = ENGINE_STATE_DRAG;

            towards_drag();
            break;

        case ENGINE_STATE_TO_SNAP:
            widget_engine_state = ENGINE_STATE_SNAP;
            break;
        }

    switch (widget_engine_state)
    {
    case ENGINE_STATE_DRAG:
    {
        struct geometry buffer = drag_release;

        const double tx = (mouse_x - drag_offset_x)/camera.sx;
        const double ty = (mouse_y - drag_offset_y)/camera.sy;

        buffer.dx = cos(camera.a)*tx+sin(camera.a)*ty;
        buffer.dy = -sin(camera.a)*tx+cos(camera.a)*ty;

        wg_bezier_set(current_hover, &buffer);

        break;
    }

    case ENGINE_STATE_TO_DRAG:
        towards_drag();

        break;
    }
}

// Make a work queue with only the widgets that have an update method.
struct work_queue* widget_engine_widget_work()
{
    struct work_queue* work_queue = work_queue_create();

    if (widget_engine_state == ENGINE_STATE_TABBED_OUT)
        return work_queue;

    for (struct wg_internal* zone = root_board->head; zone; zone = zone->next)
        work_queue_push(work_queue, wg_bezier_update, zone);

    for (struct wg_internal* zone = root_board->head; zone; zone = zone->next)
        for (struct wg_internal* piece = zone->head; piece; piece = piece->next)
            work_queue_push(work_queue, wg_bezier_update, piece);

    for (struct wg_internal* frame = root_hud->head; frame; frame = frame->next)
    {
        work_queue_push(work_queue, wg_bezier_update, frame);

        for (struct wg_internal* hud = frame->head; hud; hud = hud->next)
            work_queue_push(work_queue, wg_bezier_update, hud);
    }

    return work_queue;
}

// Handle events by calling all widgets that have a event handler.
void widget_engine_event_handler()
{
    // TODO: Incorperate to the threadpool?

    if (widget_engine_state == ENGINE_STATE_TABBED_OUT)
        if (current_event.type != ALLEGRO_EVENT_DISPLAY_SWITCH_IN)
            return;
        else
            widget_engine_state = ENGINE_STATE_IDLE;

    for (struct wg_internal* zone = root_board->head; zone; zone = zone->next)
        if(zone->jumptable->event_handler)
			zone->jumptable->event_handler(wg_public(zone));

    for (struct wg_internal* zone = root_board->head; zone; zone = zone->next)
        for (struct wg_internal* piece = zone->head; piece; piece = piece->next)
            if (piece->jumptable->event_handler)
                piece->jumptable->event_handler(wg_public(piece));

    for (struct wg_internal* frame = root_hud->head; frame; frame = frame->next)
    {
        if (frame->jumptable->event_handler)
            frame->jumptable->event_handler(wg_public(frame));

        for (struct wg_internal* hud = frame->head; hud; hud = hud->next)
            if (hud->jumptable->event_handler)
                hud->jumptable->event_handler(wg_public(hud));
    }

    switch (current_event.type)
    {
    case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
        if (current_hover != last_click)
        {
            if (last_click)
                call_click_off(last_click);

            last_click = current_hover;
        }

        if (!current_hover)
        {
            if (widget_engine_state == ENGINE_STATE_IDLE)
                if(current_event.mouse.button == 1)
                {
                    widget_engine_state = ENGINE_STATE_EMPTY_DRAG;

                    wg_bezier_interupt(&camera);
                }

            break;
        }

        if (current_event.mouse.button == 2)
        {
            call_right_click(current_hover);
        }
        
        if (current_event.mouse.button == 1)
        {
            transition_timestamp = current_timestamp + drag_threshold;

            widget_engine_state = ENGINE_STATE_PRE_DRAG_THRESHOLD;

            drag_offset_x = mouse_x - current_hover->dx;
            drag_offset_y = mouse_y - current_hover->dy;

            geometry_copy(&drag_release,&current_hover->dest);
        }
       
        break;

    case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
        process_mouse_up(current_event.mouse.button,true);
        break;

    case ALLEGRO_EVENT_MOUSE_AXES:
        if (widget_engine_state == ENGINE_STATE_EMPTY_DRAG)
        {
            camera.x += current_event.mouse.dx;
            camera.y += current_event.mouse.dy;

            camera.sx += 0.01 * current_event.mouse.dz;
            camera.sy += 0.01 * current_event.mouse.dz;

            camera.sx = camera.sx < 0.2 ? 0.2 : camera.sx;
            camera.sy = camera.sy < 0.2 ? 0.2 : camera.sy;
        }
        else if (current_hover && (
            widget_engine_state == ENGINE_STATE_POST_DRAG_THRESHOLD
            || widget_engine_state == ENGINE_STATE_DRAG))
                call_left_held(current_hover);

        break;

    case ALLEGRO_EVENT_DISPLAY_SWITCH_OUT:
        process_mouse_up(0,false);
        process_mouse_up(1,false);

        widget_engine_state = ENGINE_STATE_TABBED_OUT;
    }
}

/*********************************************/
/*          General LUA Functions            */
/*********************************************/

// Set the widget keyframe (singular) clears all current keyframes 
static int set_keyframe(lua_State* L)
{
    struct wg_internal* const wg = (struct wg_internal*) luaL_checkudata(L, -2, "widget_mt");

    struct geometry geometry;

	geometry_default(&geometry);

    lua_getgeometry(-1, &geometry);

    wg_bezier_set(wg, &geometry);

    return 0;
}

// Reads a transform from the stack and appends to the end of its current path
static int push_keyframe(lua_State* L)
{
    struct wg_internal* const wg = (struct wg_internal*)luaL_checkudata(L, -2, "widget_mt");
    luaL_checktype(L, -1, LUA_TTABLE);

    struct geometry geometry;
    geometry_default(&geometry);

    lua_getgeometry(-1, &geometry);

    geometry_copy(&wg->dest, &geometry);
    lua_getfield(L, -1, "t");

    if (lua_type(lua_state, -1) == LUA_TNUMBER)
        wg->t = lua_tonumber(lua_state, -1);
    else
        wg->t = current_timestamp;

    return 0;
}

// Pushes the type of widget onto the stack.
static int push_type(lua_State* L)
{
    struct wg_internal* const wg = (struct wg_internal*)luaL_checkudata(L, -2, "widget_mt");

    lua_pushlightuserdata(L, wg->jumptable);

    return 1;
}

// Pushes the class of widget onto the stack.
static int push_class(lua_State* L)
{
    struct wg_internal* const wg = (struct wg_internal*)luaL_checkudata(L, -2, "widget_mt");

    if (wg->type == WG_ZONE)
        lua_pushstring(L, "zone");
    else if (wg->type == WG_PIECE)
        lua_pushstring(L, "piece");
    else if (wg->type == WG_HUD)
        lua_pushstring(L, "hud");
    else
        lua_pushnil(L);

    return 1;
}

/*********************************************/
/*          Big Three LUA Callbacks          */
/*********************************************/

// General widget garbage collection
static int wg_gc(lua_State* L)
{
    // WARNING: __gc is called before freeing udata but after removing from weak references
    // In particular, the entry is already removed from the "widgets" global table.
    // I've removed function calls from "prevent stale points" for this reasion.
    // This means a hover_start can be called without an accompaning hover_end.
    struct wg_internal* const wg = (struct wg_internal*)luaL_checkudata(L, 1, "widget_mt");

    if (wg->jumptable->gc)
        wg->jumptable->gc(wg_public(wg));

    // Make sure we don't get stale pointers
    prevent_stale_pointers(wg);

    //wg_remove(wg);

    return 0;
}

// General widget index method
static int wg_index(lua_State* L)
{
    static const struct {
        const char* key;
        const lua_CFunction function;
        bool call;
    } lookup[] = {
        {"set_keyframe",set_keyframe,false},
        {"push_keyframe",push_keyframe,false},
        {"class",push_class,true},
        {"type",push_type,true},
        {NULL,NULL,false},
    };

    struct wg_internal* const wg = (struct wg_internal*)luaL_checkudata(L, -2, "widget_mt");

    if (lua_type(L, -1) == LUA_TSTRING)
    {
        const char* key = lua_tostring(L, -1);

        for (size_t i = 0; lookup[i].key; i++)
            if (strcmp(lookup[i].key, key) == 0)
            {
                if (lookup[i].call)
                    return lookup[i].function(L);

                lua_pushcfunction(L, lookup[i].function);
                return 1;
            }

        // TODO: Improve
        if (strcmp("x", key) == 0)
        {
            lua_pushnumber(L, wg->x);
            return 1;
        }
        else if (strcmp("y", key) == 0)
        {
            lua_pushnumber(L, wg->y);
            return 1;
        }

        // wg_index gets worse every update.
        if (wg_is_branch(wg))
        {
            if (wg->type == WG_ZONE)
            {
                if (strcmp("meeple", key) == 0)
                {
                    lua_pushcfunction(L, meeple_new);
                    return 1;
                }   
            }

            if (wg->type == WG_FRAME)
            {
                if (strcmp("button", key) == 0)
                {
                    lua_pushcfunction(L, button_new);
                    return 1;
                }
                else if (strcmp("counter", key) == 0)
                {
                    lua_pushcfunction(L, counter_new);
                    return 1;
                }
                else if (strcmp("text_entry", key) == 0)
                {
                    lua_pushcfunction(L, text_entry_new);
                    return 1;
                }
                else if (strcmp("slider", key) == 0)
                {
                    lua_pushcfunction(L, slider_new);
                    return 1;
                }
                else if (strcmp("drop_down", key) == 0)
                {
                    lua_pushcfunction(L, drop_down_new);
                    return 1;
                }
                else if (strcmp("tile_selector", key) == 0)
                {
                    lua_pushcfunction(L, tile_selector_new);
                    return 1;
                }
            }                
        }
    }

    if (wg->jumptable->index)
    {
        const int output = wg->jumptable->index(L);

        if (output >= 0)
            return output;
    }

    lua_getfenv(L, -2);
    lua_replace(L, -3);

    lua_gettable(L, -2);

    if (lua_isnil(L, -1))
        return 0;

    return 1;
}

// General widget newindex method
static int wg_newindex(lua_State* L)
{
    struct wg_internal* const wg = (struct wg_internal*)luaL_checkudata(L, -3, "widget_mt");

    if (wg->jumptable->newindex)
    {
        const int output = wg->jumptable->newindex(L);

        if (output >= 0)
            return output;
    }

    lua_getfenv(L, -3);
    lua_replace(L, -4);

    lua_settable(L, -3);

    return 0;
}

/*********************************************/
/*               Widget Style                */
/*********************************************/

struct widget_pallet primary_pallet, secondary_pallet;

static void style_init()
{
    primary_pallet = (struct widget_pallet)
    {
        .main = al_map_rgb(72, 91, 122),
        .highlight = al_map_rgb(114,136,173),
        .recess = al_map_rgb(0x6B,0x81,0x8C),

        .edge = al_map_rgb(0,0,0),
        .edge_radius = 10,
        .edge_width = 2,

        .activated = al_map_rgb(255,255,255),
        .deactivated = al_map_rgb(128,128,128),
        .font = resource_manager_font(FONT_ID_SHINYPEABERRY),
    };

    memcpy_s(&secondary_pallet, sizeof(struct widget_pallet),
        &primary_pallet, sizeof(struct widget_pallet));
}

/*********************************************/
/*           Widget Engine Inits             */
/*********************************************/

static void metatables_init()
{
    // Make the widget metatable
    luaL_newmetatable(lua_state, "widget_mt");

    lua_pushcfunction(lua_state, wg_index);
    lua_setfield(lua_state, -2, "__index");

    lua_pushcfunction(lua_state, wg_newindex);
    lua_setfield(lua_state, -2, "__newindex");

    lua_pushcfunction(lua_state, wg_gc);
    lua_setfield(lua_state, -2, "__gc");

    lua_pop(lua_state, 2);
}

// Initalize the Widget Engine
void widget_engine_init()
{
    metatables_init();

    init_roots();
 
    // Set empty pointers to NULL
    current_drop = NULL;
    current_hover = NULL;
    last_click = NULL;

    // Arbitarly set snap to something nonzero
    snap_offset_x = 5;
    snap_offset_y = 5;

    onscreen_shader_init();
    offscreen_shader_init();

    style_init();
    camera_init();
    zone_and_piece_init();
}

/*********************************************/
/*              Widget Allocs                */
/*********************************************/

static void wg_alloc_standardize(enum wg_type type)
{
    if (!lua_istable(lua_state, -1))
        lua_createtable(lua_state, 0, 0);

    lua_pushnumber(lua_state, (type == WG_ZONE || type == WG_PIECE));
    lua_setfield(lua_state, -2, "c");
}

void wg_alloc_wire_in(struct wg_internal* const wg)
{
    void* parent = lua_topointer(lua_state, -3);
    lua_pushroot(lua_state, wg);
    lua_getfenv(lua_state, -1);

    if (wg_is_leaf(wg))
    {
        wg_append((struct wg_internal*) parent, wg);

        lua_getfield(lua_state, -1, "leaves");
    }
    else
    {
        root_append((struct root*) parent, wg);

        lua_getfield(lua_state, -1, "branches");
    }

    lua_pushlightuserdata(lua_state, wg);
    lua_pushvalue(lua_state, -5);
    lua_settable(lua_state, -3);
    lua_pop(lua_state, 3);
}

// Inputs a lua stack of either parent or parent and fenv and outpus parent, fenv, and widget
static struct wg_internal* wg_alloc(enum wg_type type, size_t size)
{
    wg_alloc_standardize(type);

    size += sizeof(struct wg_header);
    size += sizeof(struct wg_jumptable_base*);

    struct wg_internal* const widget = lua_newuserdata(lua_state, size);

    if (!widget)
        return NULL;

    *widget = (struct wg_internal){ .type = type };

    // Set metatable
    luaL_getmetatable(lua_state, "widget_mt");
    lua_setmetatable(lua_state, -2);

    // Wire in
    wg_alloc_wire_in(widget);

    // Process keyframes and tweener
    struct geometry geometry;
	geometry_default(&geometry);

    lua_getgeometry(-2, &geometry);
    lua_cleangeometry(-2);
    wg_bezier_set(widget, &geometry);

    // Set fenv
    lua_pushvalue(lua_state, -2);
    lua_setfenv(lua_state, -2);

    return widget;
}

struct wg_zone* wg_alloc_zone(size_t size, struct wg_jumptable_zone* jumptable)
{
    struct wg_zone_internal* wg = (struct wg_zone_internal*)wg_alloc(WG_ZONE, size);

    wg->draggable = false;
    wg->snappable = false;
    wg->jumptable = (struct wg_jumptable_zone*)jumptable;

    wg->valid_move = false;
    wg->highlighted = false;
    wg->nominated = false;

    return (struct wg_zone*)wg_public((struct wg_internal*)wg);
}

struct wg_piece* wg_alloc_piece(size_t size, struct wg_jumptable_piece* jumptable)
{
    struct wg_piece_internal* wg = (struct wg_piece_internal*)wg_alloc(WG_PIECE, size);

    wg->draggable = true;
    wg->snappable = false;
    wg->jumptable = (struct wg_jumptable_piece*)jumptable;

    struct geometry geometry;
    geometry_copy(&geometry, &wg->parent->dest);

    geometry.hh = wg->dest.hh;
    geometry.hw = wg->dest.hw;

    wg_bezier_set(wg, &geometry);

    return (struct wg_piece*)wg_public((struct wg_internal*)wg);
}

struct wg_frame* wg_alloc_frame(size_t size, struct wg_jumptable_frame* jumptable)
{
    struct wg_frame_internal* wg = (struct wg_frame_internal*)wg_alloc(WG_FRAME, size);

    wg->draggable = false;
    wg->snappable = false;
    wg->jumptable = (struct wg_jumptable_frame*)jumptable;

    wg->pallet = &primary_pallet;

    return (struct wg_frame*)wg_public((struct wg_internal*)wg);
}

struct wg_hud* wg_alloc_hud(size_t size, struct wg_jumptable_hud* jumptable)
{
    struct wg_hud_internal* wg = (struct wg_hud_internal*)wg_alloc(WG_HUD, size);

    wg->draggable = false;
    wg->snappable = false;
    wg->jumptable = jumptable;

    wg->hud_state = HUD_IDLE;
    wg->pallet = &primary_pallet;

    return (struct wg_hud*)wg_public((struct wg_internal*)wg);
}
