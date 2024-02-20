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

#ifdef WIDGET_DEBUG_DRAW
#include <allegro5/allegro_primitives.h>
#endif

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

/*********************************************/
/*            Widget Memory Layout           */
/*********************************************/

// Header
// Base (first entry is keyframe)
// Zone/Piece Particulars 
// General Particulars

//TODO: should this be public?
enum wg_class
{
    // In order of render
    WG_ZONE,
    WG_PIECE,
    WG_HUD,

    WG_CLASS_CNT = 3,

    //WG_BASE,

    // For easy iteration
    WG_CLASS_START = WG_ZONE,
    WG_CLASS_END = WG_CLASS_START + WG_CLASS_CNT,
};

struct wg_header
{
	enum wg_class class;

	union {
		struct wg_jumptable_base* base;
		struct wg_jumptable_zone* zone;
		struct wg_jumptable_piece* piece;
        struct wg_jumptable_hud* hud;
	} jumptable;

	struct wg_base_internal* next;
	struct wg_base_internal* previous;

    struct
    {
        // Keypoints
        size_t used, allocated;
        struct keyframe* keypoints; 

        // Looping data
        size_t looping_idx;
        double looping_time;
    }tweener;
};

struct wg_base_internal
{
	struct wg_header;
	struct wg_base;
};

struct wg_zone_internal
{
	struct wg_header;
	struct wg_zone;
};

struct wg_piece_internal
{
	struct wg_header;
	struct wg_piece;
};

struct wg_hud_internal
{
    struct wg_header;
    struct wg_hud;
};

static inline struct wg_base_internal* get_internal(struct wg_base* base)
{
     return (struct wg_base_internal*) ((char*) base - sizeof(struct wg_header) );
}

static inline struct wg_base* downcast(struct wg_base_internal* internal)
{
    return (struct wg_base*) ((char*)internal + sizeof(struct wg_header));
}

static inline struct keyframe* get_keyframe(struct wg_base_internal* wg)
{
    return (struct keyframe*) downcast(wg);
}


/*********************************************/
/*          Widget Queue Methods             */
/*********************************************/

static struct wg_base_internal* queue_head[WG_CLASS_CNT];
static struct wg_base_internal* queue_tail[WG_CLASS_CNT];

// Pop a widget out of the engine
static void queue_pop(struct wg_base_internal* const widget)
{
    if (widget->next)
        widget->next->previous = widget->previous;
    else if(queue_tail[widget->class] == widget)
        queue_tail[widget->class] = widget->previous;

    if (widget->previous)
        widget->previous->next = widget->next;
    else if (queue_head[widget->class] == widget)
        queue_head[widget->class] = widget->next;
}

// Insert the first widget behind the second
static void queue_insert(struct wg_base_internal* mover, struct wg_base_internal* target)
{
    // TODO: Add some type checking, not every mover type should be able to go with every target type.
    // Preformance issue, usesd in the drawing loop. (But only twice).

    // Assumes mover is outside the list.
    // I.e. that mover's next and previous are null.
    mover->next = target;

    // If the second is null append the first to the head
    if (target)
    {
        if (target->previous)
            target->previous->next = mover;
        else
            queue_head[target->class] = mover;

        target->previous = mover;
    }
    else
    {
        mover->previous = queue_tail[mover->class];

        if (queue_tail[mover->class])
            queue_tail[mover->class]->next = mover;
        else
            queue_head[mover->class] = mover;

        queue_tail[mover->class] = mover;
    }
}

// Move the mover widget behind the target widget.
void widget_interface_move(struct wg_base* mover, struct wg_base* target)
{
    if (mover == target)
        return;

    // Only this function is visable to the widget writer.
    // Since poping a widget without calling gc isn't allowed.
    queue_pop(get_internal(mover));
    queue_insert(get_internal(mover), get_internal(target));
}

/*********************************************/
/*              Widget Tweener               */
/*********************************************/

static void tweener_init(struct wg_base_internal* const wg)
{
    size_t hint = 1;

    wg->tweener.used = 1;
    wg->tweener.allocated = hint;
    wg->tweener.keypoints = malloc(hint * sizeof(struct keyframe));
    wg->tweener.looping_time = -1;
    wg->tweener.looping_idx = 0;

    keyframe_copy(wg->tweener.keypoints, get_keyframe(wg));
}

static void tweener_gc(struct wg_base_internal* const wg)
{
    free(wg->tweener.keypoints);
}

static void tweener_blend_nonlooping(struct wg_base_internal* const wg)
{
    // Clean old frames
    size_t first_future_frame = 0;

    while (wg->tweener.keypoints[first_future_frame].t < current_timestamp &&
        first_future_frame < wg->tweener.used)
        first_future_frame++;

    if (first_future_frame > 1)
    {
        const size_t step = first_future_frame - 1;

        for (size_t i = step; i < wg->tweener.used; i++)
            keyframe_copy(&(wg->tweener.keypoints[(i - step)]),
                &(wg->tweener.keypoints[i]));

        wg->tweener.used -= step;

        if (wg->tweener.used == 1)
        {
            keyframe_copy(get_keyframe(wg), wg->tweener.keypoints);

            return;
        }
    }

    const double denominator = (wg->tweener.keypoints[1].t - wg->tweener.keypoints[0].t);
    const double blend = (current_timestamp - wg->tweener.keypoints[0].t) / denominator;

    keyframe_blend(get_keyframe(wg), wg->tweener.keypoints + 1, wg->tweener.keypoints, blend);
}

static void tweener_blend_looping(struct wg_base_internal* const wg)
{
    while (wg->tweener.keypoints[wg->tweener.looping_idx].t <= current_timestamp)
    {
        const size_t back_idx = (wg->tweener.looping_idx >= 1) ?
            wg->tweener.looping_idx - 1 : wg->tweener.used - 1;

        wg->tweener.keypoints[back_idx].t += wg->tweener.looping_time;

        wg->tweener.looping_idx = (wg->tweener.looping_idx != SIZE_MAX) ?
            (wg->tweener.looping_idx + 1) % wg->tweener.used : 0;
    }

    const size_t end_idx = wg->tweener.looping_idx;
    const size_t start_idx = (wg->tweener.looping_idx >= 1) ?
        (wg->tweener.looping_idx - 1) : wg->tweener.used - 1;

    const double blend = (current_timestamp - wg->tweener.keypoints[start_idx].t) /
        (wg->tweener.keypoints[end_idx].t - wg->tweener.keypoints[start_idx].t);

    keyframe_blend(get_keyframe(wg), wg->tweener.keypoints + 1, wg->tweener.keypoints, blend);
}

static void tweener_blend(struct wg_base_internal* const wg)
{
    if (wg->tweener.used > 1)
        if (wg->tweener.looping_time > 0)
            tweener_blend_looping(wg);
        else
            tweener_blend_nonlooping(wg);
}

static void tweener_set(struct wg_base_internal* const wg, struct keyframe* keypoint)
{
    wg->tweener.used = 1;
    wg->tweener.looping_time = -1;

    keyframe_copy(get_keyframe(wg), keypoint);
    keyframe_copy(wg->tweener.keypoints, keypoint);
}

static void tweener_push(struct wg_base_internal* const wg, struct keyframe* keypoint)
{
    if (keypoint->t - wg->tweener.keypoints[wg->tweener.used - 1].t < 0.01)
        return;

    if (wg->tweener.allocated <= wg->tweener.used)
    {
        const size_t new_cnt = 2 * wg->tweener.allocated;

        struct keyframe* memsafe_hande = realloc(wg->tweener.keypoints, new_cnt * sizeof(struct keyframe));

        // TODO: raise error
        if (!memsafe_hande)
            return;

        wg->tweener.keypoints = memsafe_hande;
        wg->tweener.allocated = new_cnt;
    }

    if (wg->tweener.used == 1)
        wg->tweener.keypoints[0].t = current_timestamp;

    keyframe_copy(wg->tweener.keypoints + wg->tweener.used++, keypoint);
}

static void tweener_interupt(struct wg_base_internal* const wg)
{
    if (wg->tweener.used > 1)
        tweener_blend(wg);

    wg->tweener.used = 1;
    wg->tweener.looping_time = -1;

    keyframe_copy(wg->tweener.keypoints, get_keyframe(wg));
}

static void tweener_destination(struct wg_base_internal* const wg, struct keyframe* keypoint)
{
    keyframe_copy(keypoint, wg->tweener.keypoints + wg->tweener.used - 1);
}

static void tweener_enter_loop(struct wg_base_internal* wg, double loop_offset)
{
    if (wg->tweener.used < 2)
        return;

    // TODO: Can optimize using a division to get loops
    size_t idx = 0;
    size_t loops = 0;
    const double loop_time = wg->tweener.keypoints[wg->tweener.used - 1].t - wg->tweener.keypoints[0].t + loop_offset;

    while (wg->tweener.keypoints[idx].t <= current_timestamp - ((double)loops) * loop_time)
        if (++idx == wg->tweener.used)
            loops++, idx = 0;

    if (loops)
        for (size_t i = 0; i < wg->tweener.used; i++)
            wg->tweener.keypoints[i].t += ((double)loops) * loop_time;

    wg->tweener.looping_idx = idx;
    wg->tweener.looping_time = loop_time;
}

/*********************************************/
/*          Zone and Piece Methods           */
/*********************************************/

// Managing four main callbacks
//  moves:
//  valid_move:
//  invalid_move:
//  manual_move:
//
//  Also a callback for a manual move from lua.

static struct wg_piece_internal* moving_piece;
static struct wg_zone_internal* originating_zone;

static bool auto_snap;					// When making a potential move automatically snap the piece valid zones.
static bool auto_highlight;			    // When making a potential move automatically highlight valid zone.
static bool auto_transition;			// After making a move auto transition to the zones snap.
static bool auto_self_highlight;		// Highlight the zone a piece comes from but block it from calling vaild_move.

static void zone_and_piece_init()
{
    auto_highlight = true;
    auto_snap = true;
    auto_transition = true;
    auto_self_highlight = true;

    originating_zone = NULL;
}

// Return the zones to the idle state
static void idle_zones()
{
    for (struct wg_zone_internal* wg = (struct wg_zone_internal*)queue_head[WG_ZONE];
        wg; wg = (struct wg_zone_internal*)wg->next)
    {
        wg->valid_move = false;
        wg->nominated = false;

        if (auto_snap)
            wg->snappable = false;

        if (wg->jumptable.zone->highlight_end)
            wg->jumptable.zone->highlight_end((struct wg_zone* const)downcast((struct wg_base_internal*)wg));

        wg->highlighted = false;
    }
}

static void call_moves(struct wg_piece_internal* wg)
{
    lua_getglobal(lua_state, "widgets");
    lua_getglobal(lua_state, "moves");

    if (!lua_isfunction(lua_state, -1))
    {
        lua_pop(lua_state, 2);
        return;
    }

    lua_pushlightuserdata(lua_state, wg);
    lua_gettable(lua_state, -3);

    if (wg->zone)
    {
        lua_pushlightuserdata(lua_state, get_internal((struct wg_base*) wg->zone));
        lua_gettable(lua_state, -4);

        if (auto_self_highlight)
        {
            originating_zone = (struct wg_zone_internal* const)get_internal((struct wg_base*)wg->zone);

            originating_zone->valid_move = true;

            if (auto_highlight)
            {
                originating_zone->highlighted = true;

                if (originating_zone->jumptable.zone->highlight_start)
                    originating_zone->jumptable.zone->highlight_start((struct wg_zone* const)downcast((struct wg_base_internal*)originating_zone));
            }

            if (auto_snap)
                originating_zone->snappable = true;
        }
    }
    else
    {
        lua_pushnil(lua_state);
    }

    lua_call(lua_state, 2, 1);

    if (!lua_istable(lua_state, -1))
    {
        lua_pop(lua_state, 2);
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

        if (zone->class != WG_ZONE)
        {
            lua_pop(lua_state, 1);
            continue;
        }

        zone->valid_move = true;

        if (auto_highlight)
        {
            zone->highlighted = true;

            if (zone->jumptable.zone->highlight_start)
                zone->jumptable.zone->highlight_start((struct wg_zone* const)downcast((struct wg_base_internal*)zone));
        }

        if (auto_snap)
            zone->snappable = true;

        lua_pop(lua_state, 1);
    }
    lua_pop(lua_state, 2);
  }

// Removes a piece from the zone, not from the board
static void remove_piece(struct wg_zone_internal* const zone, struct wg_piece_internal* const piece)
{
    size_t i;

    for (i = 0; i < zone->used; i++)
        if (zone->pieces[i] == (struct wg_piece*)downcast((struct wg_base_internal*)piece))
            break;

    for (size_t j = i + 1; j < zone->used; j++)
        zone->pieces[j - 1] = zone->pieces[j];

    // Will error if used==0, but the function shouldn't be called.
    zone->pieces[--zone->used] = NULL;

    piece->zone = NULL;
}

// Append a piece to a zone
static void append_piece(struct wg_zone_internal* const zone, struct wg_piece_internal* const piece)
{
    if (zone->allocated <= zone->used)
    {
        const size_t new_cnt = 1.2 * zone->allocated + 1;

        struct wg_piece** memsafe_hande = realloc(zone->pieces, new_cnt * sizeof(struct piece*));

        if (!memsafe_hande)
            return;

        zone->pieces = memsafe_hande;
        zone->allocated = new_cnt;
    }

    zone->pieces[zone->used++] = (struct wg_piece*)downcast((struct wg_base_internal*)piece);
    piece->zone = (struct wg_zone*)downcast((struct wg_base_internal*)zone);
}

// Call the (in)vaild move callback
static void moves_callback(struct wg_zone_internal* const zone, struct wg_piece_internal* const piece, bool vaild)
{
    lua_getglobal(lua_state, "widgets");

    if (vaild)
    {
        if (auto_self_highlight && zone == originating_zone)
        {
            lua_pop(lua_state, 1);
            return;
        }

        lua_getglobal(lua_state, "vaild_move");
    }
    else
        lua_getglobal(lua_state, "invaild_move");

    if (!lua_isfunction(lua_state, -1))
    {
        lua_pop(lua_state, 2);
        return;
    }

    lua_pushlightuserdata(lua_state, piece);
    lua_gettable(lua_state, -3);

    lua_pushlightuserdata(lua_state, zone);
    lua_gettable(lua_state, -4);

    lua_call(lua_state, 2, 0);
    lua_pop(lua_state, 1);
}

// Update the zone and pieces structs in responce to a move
static inline void move_piece(struct wg_zone_internal* const zone, struct wg_piece_internal* const piece)
{
    // If the piece was in a zone remove it from the old zone
    if (piece->zone)
    {
        struct wg_zone_internal* leaving_zone = (struct wg_zone_internal*)get_internal((struct wg_base*)piece->zone);
        if (leaving_zone->jumptable.zone->remove_piece)
            leaving_zone->jumptable.zone->remove_piece(piece->zone, (struct wg_piece*)downcast((struct wg_base_internal*)piece));

        remove_piece(leaving_zone, piece);
    }

    // If there is a new zone append the pice to it.
    if (zone)
    {
        struct wg_zone* entering_zone = (struct wg_zone*)downcast((struct wg_base_internal*)zone);

        if (zone->jumptable.zone->append_piece)
            zone->jumptable.zone->append_piece(entering_zone, (struct wg_piece*)downcast((struct wg_base_internal*)piece));

        append_piece(zone, piece);

        // If the auto_transition flag is set transition the piece to be over the zone.
        if (auto_transition)
        {
            struct keyframe keyframe;
            tweener_interupt((struct wg_base_internal*)piece);
            tweener_destination((struct wg_base_internal*)zone, &keyframe);
            keyframe.t = current_timestamp + 0.2;
            tweener_push((struct wg_base_internal*)piece, &keyframe);
        }
    }
}

// Process a manual move from lua
static int manual_move(lua_State* L)
{
    struct wg_piece_internal* piece = (struct wg_piece_internal*)luaL_checkudata(lua_state, -2, "widget_mt");
    struct wg_zone_internal* zone = (struct wg_zone_internal*)luaL_checkudata(lua_state, -1, "widget_mt");

    if (!piece || !zone)
        return 0;

    if (piece->class != WG_PIECE || zone->class != WG_ZONE)
        return 0;

    move_piece(zone, piece);

    return 0;
}

/*********************************************/
/*             Widget Callbacks              */
/*********************************************/

// Callback macros assumes widget's type is (struct wg_base_internal*).
// 
// Very much not happy with this solution.
// I don't feel like optimizing something I'm pretty sure I will refactor.

static void call_lua(struct wg_base_internal* const wg, const char* key, struct wg_base_internal* const obj)
{
    lua_getglobal(lua_state, "widgets");
    lua_pushlightuserdata(lua_state, wg);
    lua_gettable(lua_state, -2);
    
    // In the end this might be removable, keeping for now.
    if (lua_isnil(lua_state, -1))
    {
        lua_pop(lua_state, 3);
        return;
    }

    lua_getfenv(lua_state, -1);
    lua_getfield(lua_state, -1, key);
    
    if (lua_isnil(lua_state, -1))
    {
        lua_pop(lua_state, 4);
        return;
    }

    lua_pushvalue(lua_state, -3);

    if (obj)
    {
        lua_pushlightuserdata(lua_state, obj);
        lua_gettable(lua_state, -5);

        lua_pcall(lua_state, 2, 0, 0);
    }
    else
    {
        lua_pcall(lua_state, 1, 0, 0);
    }

    lua_pop(lua_state, 3);
}

static void call_hover_start(struct wg_base_internal* const wg, const char* key)
{
    if (strcmp(key, "hover_start") != 0)
        return;

    if (wg->class == WG_PIECE)    
    {
        call_moves((struct wg_piece_internal*)wg);
    }
    else if (wg->class == WG_HUD)
    {
        struct wg_hud_internal* hud = (struct wg_hud_internal*)wg;

        if(hud->hud_state == HUD_IDLE)
			hud->hud_state = HUD_HOVER;
    }
}

static void call_hover_end(struct wg_base_internal* const wg, const char* key)
{
    if (strcmp(key, "hover_end") != 0)
        return;

    if (wg->class == WG_PIECE)
    {
        idle_zones();
    }
    else if (wg->class == WG_HUD)
    {
        struct wg_hud_internal* hud = (struct wg_hud_internal*)wg;

        if (hud->hud_state == HUD_HOVER)
            hud->hud_state = HUD_IDLE;
    }
}

static void call_drop_start(struct wg_base_internal* const wg, const char* key, struct wg_base_internal* const obj)
{
    if (wg->class == WG_ZONE &&
        strcmp(key, "drop_start") == 0)
        ((struct wg_zone_internal* const)wg)->nominated = true;

    if (wg->class == WG_PIECE &&
        ((struct wg_piece_internal* const)wg)->zone &&
        strcmp(key, "drop_start") == 0)
        ((struct wg_piece_internal* const)wg)->zone->nominated = true;
}

static void call_drop_end(struct wg_base_internal* const wg, const char* key, struct wg_base_internal* const obj)
{
    if (wg->class == WG_ZONE &&
        strcmp(key, "drop_end") == 0)
        ((struct wg_zone_internal* const)wg)->nominated = false;

    if (wg->class == WG_PIECE &&
        ((struct wg_piece_internal* const)wg)->zone &&
        strcmp(key, "drop_end") == 0)
        ((struct wg_piece_internal* const)wg)->zone->nominated = false;
}

static void call_drag_end_drop(struct wg_base_internal* const wg, const char* key, struct wg_base_internal* const obj)
{
    if (wg->class == WG_ZONE &&
        strcmp(key, "drag_end_drop") == 0)
    {
        if (obj->class != WG_PIECE)
            return;

        struct wg_zone_internal* const zone = (struct wg_zone_internal* const) wg;
        struct wg_piece_internal* const piece = (struct wg_piece_internal* const) obj;

        if (!zone->valid_move)
        {
            moves_callback(zone, piece, false);
            return;
        }

        if (zone == originating_zone)
            return;

        move_piece(zone, piece);
        moves_callback(zone, piece, true);
        idle_zones();
        call_moves(piece);
        
        zone->nominated = false;
        originating_zone = NULL;
    }

    if (wg->class == WG_PIECE &&
        strcmp(key, "drag_end_drop") == 0)
    {
        if (!((struct wg_piece_internal* const)wg)->zone)
            return;

        if (obj->class != WG_PIECE)
            return;

        // I love typecasting
        struct wg_zone_internal* const zone = (struct wg_zone_internal* const) get_internal((struct wg_base*) ((struct wg_piece_internal* const)wg)->zone);
        struct wg_piece_internal* const piece = (struct wg_piece_internal* const)obj;

        if (!zone->valid_move)
        {
            moves_callback(zone, piece, false);
            return;
        }

        if (zone == originating_zone)
            return;

        move_piece(zone, piece);
        moves_callback(zone, piece, true);
        idle_zones();
        call_moves(piece);

        zone->nominated = false;
        originating_zone = NULL;
    }
}

#define call(widget,method) \
    do{ \
        call_hover_start(widget, #method);\
        call_hover_end(widget, #method);\
        if((widget)->jumptable.base-> ## method)\
			(widget)->jumptable.base-> ## method(downcast(widget)); \
        call_lua(widget, #method, NULL);\
    }while(0); 

#define call_2(widget,method,obj) \
    do{ \
        call_drop_start(widget, #method,obj);\
        call_drop_end(widget, #method,obj);\
		call_drag_end_drop(widget, #method, obj);\
        if((widget)->jumptable.base-> ## method)\
			(widget)->jumptable.base-> ## method(downcast(widget),downcast(obj)); \
        call_lua(widget, #method, obj);\
     }while(0); 


/*********************************************/
/*               Camera Tweener              */
/*********************************************/

static struct wg_base_internal camera;

static void camera_compose_transform(ALLEGRO_TRANSFORM* const trans, const double blend)
{
    ALLEGRO_TRANSFORM buffer;

    const double blend_x = camera.x * blend;
    const double blend_y = camera.y * blend;
    const double blend_sx = camera.sx * blend + (1 - blend);
    const double blend_sy = camera.sy * blend + (1 - blend);
    const double blend_a = camera.a * blend;

    al_build_transform(&buffer,
        blend_x, blend_y,
        blend_sx, blend_sy,
        blend_a);

    al_compose_transform(trans, &buffer);
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
static struct wg_base_internal* last_click;
static struct wg_base_internal* current_hover;
static struct wg_base_internal* current_drop;

/*********************************************/
/*                  Shaders                  */
/*********************************************/

// Pick (Offscreen drawing) Variables
static ALLEGRO_SHADER* offscreen_shader;
static ALLEGRO_BITMAP* offscreen_bitmap;

static ALLEGRO_SHADER* onscreen_shader;

// Handle picking mouse inputs using off screen drawing.
static inline struct wg_base_internal* pick(int x, int y)
{
    ALLEGRO_BITMAP* original_bitmap = al_get_target_bitmap();

    al_set_target_bitmap(offscreen_bitmap);
    al_set_clipping_rectangle(x - 1, y - 1, 3, 3);
    glDisable(GL_STENCIL_TEST);

    al_clear_to_color(al_map_rgba(0, 0, 0, 0));

    al_use_shader(offscreen_shader);

    size_t picker_index = 1;
    size_t pick_buffer;
    float color_buffer[3];

    const bool hide_hover = hover_on_top();

    if (hide_hover)
        queue_pop(current_hover);

    for (size_t type = 0; type < WG_CLASS_CNT; type++)
		for (struct wg_base_internal* wg = queue_head[type]; wg; wg = wg->next, picker_index++)
		{
			pick_buffer = picker_index;

			for (size_t i = 0; i < 3; i++)
			{
				color_buffer[i] = ((float)(pick_buffer % 200)) / 200;
				pick_buffer /= 200;
			}

			al_set_shader_float_vector("picker_color", 3, color_buffer, 1);

			ALLEGRO_TRANSFORM buffer;
			keyframe_build_transform((const struct keyframe* const)get_keyframe(wg), &buffer);
			camera_compose_transform(&buffer, wg->c);
			al_use_transform(&buffer);

			wg->jumptable.base->mask(downcast(wg));
		}

    al_set_target_bitmap(original_bitmap);

    al_unmap_rgb_f(al_get_pixel(offscreen_bitmap, x, y),
        color_buffer, color_buffer + 1, color_buffer + 2);

    size_t index = round(200 * color_buffer[0]) +
        200 * round(200 * color_buffer[1]) +
        40000 * round(200 * color_buffer[2]);

    if (index == 0)
    {
        if (hide_hover)
            queue_insert(current_hover, current_hover->next);

        return NULL;
    }

    struct wg_base_internal* widget = NULL;

    for (size_t type = 0; type < WG_CLASS_CNT && index > 0; type++)
    {
        widget = queue_head[type];

        while (widget && index-- > 1)
            widget = widget->next;
    }

    if (hide_hover)
        queue_insert(current_hover, current_hover->next);

    return widget;
}

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

    tweener_blend(&camera);
}

/*********************************************/
/*      General Widget Engine Methods        */
/*********************************************/

// Drag and Snap Variables
static struct keyframe drag_release;
static double drag_offset_x, drag_offset_y; // coordinates system match current_hover (screen or world)
static double snap_offset_x, snap_offset_y; // coordinates system match snap (screen or world)
static const double snap_speed = 1000; // px per sec
static const double drag_threshold = 0.2;

// Prevents current_hover, last_click, and current_drag becoming stale
static void prevent_stale_pointers(struct wg_base_internal* const ptr)
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
static void draw_widget(const struct wg_base_internal* const wg)
{
    //al_set_shader_float("variation", internal->variation);

    // You don't actually have to send this everytime, should track a count of materials that need it.
    if (wg->half_width != 0 && wg->half_height != 0)
    {
        const float dimensions[2] = { 1.0 / wg->half_width, 1.0 / wg->half_height };
        al_set_shader_float_vector("object_scale", 2, dimensions, 1);
    }

    ALLEGRO_TRANSFORM buffer;
    keyframe_build_transform((struct keyframe* const) get_keyframe(wg), (ALLEGRO_TRANSFORM* const ) & buffer);
    camera_compose_transform(&buffer, wg->c);
    al_use_transform(&buffer);

    material_apply(NULL);
    glDisable(GL_STENCIL_TEST);

    wg->jumptable.base->draw((const struct wg_base* const) downcast(wg));

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
    struct keyframe keyframe = drag_release;

    tweener_interupt(current_hover);

    keyframe.dx = mouse_x - drag_offset_x;
    keyframe.dy = mouse_y - drag_offset_y;
    keyframe.t  = current_timestamp + 0.1;

    tweener_push(current_hover, &keyframe);
}

// Updates and calls any callbacks for the last_click, current_hover, current_drop pointers
static inline void update_drag_pointers()
{
    // What pointer the picker returns depends on if something is being dragged.
    // If nothing is being dragged the pointer is what's being hovered.
    // If something is being dragged the pointer is what's under the dragged widget.
    //  (The widget under the drag is called the drop).
    struct wg_base_internal* const new_pointer = pick(mouse_x, mouse_y);

    if (current_hover != new_pointer && (
        widget_engine_state == ENGINE_STATE_IDLE ||
        widget_engine_state == ENGINE_STATE_HOVER))
    {
        if (current_hover)
            call(current_hover, hover_end);

        if (new_pointer)
        {
            call(new_pointer, hover_start);

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
            call_2(current_drop, drop_end, current_hover);
        }

        if (new_pointer)
        {
            call_2(new_pointer, drop_start, current_hover);

			if (new_pointer->snappable)
			{
                tweener_interupt(current_hover);

				struct keyframe snap_target;
                tweener_destination(new_pointer, &snap_target);

				snap_target.dx += snap_offset_x;
				snap_target.dy += snap_offset_y;
				snap_target.t = current_timestamp + 0.1;

                tweener_push(current_hover, &snap_target);

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

    keyframe_build_transform(get_keyframe(get_internal(wg)), &transform);

    // WARNING: the inbuilt invert only works for 2D transforms
    al_invert_transform(&transform);
    //invert_transform_3D(&transform);

    al_transform_coordinates(&transform, &_x, &_y);

    *x = _x;
    *y = _y;
}

// Check that a widget has the right jumptable
struct wg_base* check_widget(struct wg_base* wg, const struct wg_jumptable_base* const jumptable)
{
    return ((get_internal(wg))->jumptable.base == jumptable) ? wg : NULL;
}

struct wg_base* check_widget_lua(int idx, const struct wg_jumptable_base* const jumptable)
{
    if (lua_type(lua_state, idx) != LUA_TUSERDATA)
        return NULL;

    struct wg_base_internal* wg = (struct wg_base_internal*) luaL_checkudata(lua_state, idx, "widget_mt");

    if (!wg || wg->jumptable.base != jumptable)
        return NULL;

    return downcast(wg);
}

// Change widget_engine_state __like__ ALLEGRO_EVENT_MOUSE_BUTTON_UP occurred.
static void process_mouse_up(unsigned int button, bool allow_drag_end_drop)
{
    if (button != 1)
        return;

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
        call(current_hover, left_click);
        if (current_hover)
            call(current_hover, left_click_end);

        // This means hover_start can be called twice without a hover_end
        // Not commited to it (should filter for existance of drag_start?)
        if (current_hover)
            call(current_hover, hover_start);
        break;

    case ENGINE_STATE_POST_DRAG_THRESHOLD:
        call(current_hover, left_click_end);

        // This means hover_start can be called twice without a hover_end
        // Not commited to it
        if (current_hover)
            call(current_hover, hover_start);
        break;

    case ENGINE_STATE_DRAG:
    case ENGINE_STATE_SNAP:
    case ENGINE_STATE_TO_SNAP:
    case ENGINE_STATE_TO_DRAG:
        tweener_interupt(current_hover);

        drag_release.t = current_timestamp + 0.1;

        tweener_push(current_hover, &drag_release);

        if (current_drop && allow_drag_end_drop)
        {
            call_2(current_drop, drag_end_drop, current_hover);
            if (current_hover)
                call(current_hover, drag_end_no_drop);
        }
        else
            call(current_hover, drag_end_no_drop);

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
    const bool hide_hover = hover_on_top();

    if (hide_hover)
        queue_pop(current_hover);

    // Maybe add a second pass for stencil effect?
    for (enum wg_class class = WG_CLASS_START; class < WG_CLASS_END; class++)
		for (struct wg_base_internal* widget = queue_head[class]; widget; widget = widget->next)
			draw_widget((struct wg_base_internal*) widget);

    if (hide_hover)
    {
        queue_insert(current_hover, current_hover->next);
        draw_widget(current_hover);
    }

    if (widget_engine_state == ENGINE_STATE_TABBED_OUT)
    {
        al_use_transform(&identity_transform);

        ALLEGRO_DISPLAY* display = al_get_current_display();

        al_draw_filled_rectangle(0, 0, 
            al_get_display_width(display), al_get_display_height(display),
            al_map_rgba_f(0,0,0, 0.5));
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
    al_draw_textf(debug_font, al_map_rgb_f(0, 1, 0), 10, 130, ALLEGRO_ALIGN_LEFT,
        "Head: %p, Tail: %p",
        queue_head, queue_tail);
#endif
}

// Update the widget engine state
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
                call(current_hover, drag_start);

                towards_drag();
                widget_engine_state = ENGINE_STATE_TO_DRAG;
            }
            else
            {
                call(current_hover, left_click);

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
        struct keyframe buffer = drag_release;

        const double tx = (mouse_x - drag_offset_x)/camera.sx;
        const double ty = (mouse_y - drag_offset_y)/camera.sy;

        buffer.dx = cos(camera.a)*tx+sin(camera.a)*ty;
        buffer.dy = -sin(camera.a)*tx+cos(camera.a)*ty;

        tweener_set(current_hover, &buffer);

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
    // Since the update method doesn't change maybe we should have a static queue?

    struct work_queue* work_queue = work_queue_create();

	for (enum wg_class class = WG_CLASS_START; class < WG_CLASS_END; class++)
		for (struct wg_base_internal* widget = queue_head[class]; widget; widget = widget->next)
		{
			work_queue_push(work_queue,tweener_blend,widget);

			if(widget_engine_state != ENGINE_STATE_TABBED_OUT &&
                widget->jumptable.base->update)
				work_queue_push(work_queue, widget->jumptable.base->update, downcast(widget));
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

    for (enum wg_class class = WG_CLASS_START; class < WG_CLASS_END; class++)
		for (struct wg_base_internal* widget = queue_head[class]; widget; widget = widget->next)
			if(widget->jumptable.base->event_handler)
				widget->jumptable.base->event_handler(downcast(widget));

    switch (current_event.type)
    {
    case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
        if (current_hover != last_click)
        {
            if (last_click)
                call(last_click, click_off);

            last_click = current_hover;
        }

        if (!current_hover)
        {
            if (widget_engine_state == ENGINE_STATE_IDLE)
                if(current_event.mouse.button == 1)
                {
                    widget_engine_state = ENGINE_STATE_EMPTY_DRAG;

                    tweener_interupt(&camera);
                }

            break;
        }

        if (current_event.mouse.button == 2)
        {
            call(current_hover, right_click);
        }
        
        if (current_event.mouse.button == 1)
        {
            transition_timestamp = current_timestamp + drag_threshold;

            widget_engine_state = ENGINE_STATE_PRE_DRAG_THRESHOLD;

            drag_offset_x = mouse_x - current_hover->dx;
            drag_offset_y = mouse_y - current_hover->dy;

            tweener_destination(current_hover, &drag_release);
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
        break;

    case ALLEGRO_EVENT_DISPLAY_SWITCH_OUT:
        process_mouse_up(0,false);
        process_mouse_up(1,false);

        widget_engine_state = ENGINE_STATE_TABBED_OUT;
    }
}

/*********************************************/
/*               LUA interface               */
/*********************************************/

// Set the widget keyframe (singular) clears all current keyframes 
static int set_keyframe(lua_State* L)
{
    struct wg_base_internal* const wg = (struct wg_base_internal*) luaL_checkudata(L, -2, "widget_mt");

    struct keyframe keyframe;

    keyframe_default(&keyframe);
    lua_getkeyframe(-1, &keyframe);

    tweener_set(wg, &keyframe);

    return 0;
}

// Reads a transform from the stack and appends to the end of its current path
static int push_keyframe(lua_State* L)
{
    struct wg_base_internal* const wg = (struct wg_base_internal*)luaL_checkudata(L, -2, "widget_mt");
    luaL_checktype(L, -1, LUA_TTABLE);

    struct keyframe keyframe;
    keyframe_default(&keyframe);

    lua_getkeyframe(-1, &keyframe);

    tweener_push(wg, &keyframe);
    return 0;
}

// Pushes the type of widget onto the stack.
static int push_type(lua_State* L)
{
    struct wg_base_internal* const wg = (struct wg_base_internal*)luaL_checkudata(L, -2, "widget_mt");

    lua_pushlightuserdata(L, wg->jumptable.base);

    return 1;
}

// Pushes the class of widget onto the stack.
static int push_class(lua_State* L)
{
    struct wg_base_internal* const wg = (struct wg_base_internal*)luaL_checkudata(L, -2, "widget_mt");

    if (wg->class == WG_ZONE)
        lua_pushstring(L, "zone");
    else if (wg->class == WG_PIECE)
        lua_pushstring(L, "piece");
    else if (wg->class == WG_HUD)
        lua_pushstring(L, "hud");
    else
        lua_pushnil(L);

    return 1;
}

// Push camera
static int camera_push(lua_State* L)
{
    luaL_checktype(L, -1, LUA_TTABLE);

    struct keyframe keyframe;
    keyframe_default(&keyframe);

    lua_getkeyframe(-1, &keyframe);

    tweener_push(&camera, &keyframe);
    return 0;
}

// Set camera
static int camera_set(lua_State* L)
{
    luaL_checktype(L, -1, LUA_TTABLE);

    struct keyframe keyframe;

    keyframe_default(&keyframe);
    lua_getkeyframe(-1, &keyframe);

    tweener_set(&camera, &keyframe);

    return 0;
}

// General widget garbage collection
static int gc(lua_State* L)
{
    // WARNING: __gc is called before freeing udata but after removing from weak references
    // In particular, the entry is already removed from the "widgets" global table.
    // I've removed function calls from "prevent stale points" for this reasion.
    // This means a hover_start can be called without an accompaning hover_end.
    struct wg_base_internal* const wg = (struct wg_base_internal*)luaL_checkudata(L, 1, "widget_mt");

    if (wg->jumptable.base->gc)
        wg->jumptable.base->gc(downcast(wg));

    if (wg->class == WG_ZONE)
        free(((struct wg_zone_internal* const)wg)->pieces);

    // Make sure we don't get stale pointers
    prevent_stale_pointers(wg);

    queue_pop(wg);

    return 0;
}

// General widget index method
static int index(lua_State* L)
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

    struct wg_base_internal* const wg = (struct wg_base_internal*)luaL_checkudata(L, -2, "widget_mt");

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

        if (wg->class == WG_ZONE)
        {
            struct wg_zone_internal* const zone = (struct wg_zone_internal* const) wg;

            if (strcmp(key, "pieces") == 0)
            {
                lua_getglobal(L, "widgets");
                lua_createtable(L, (int) zone->used, 0);

                for (size_t i = 0; i < zone->used; i++)
                {
                    lua_pushnumber(L, i + 1);
                    lua_pushlightuserdata(L, get_internal((struct wg_base*) zone->pieces[i]));
                    lua_gettable(L, -4);
                    lua_settable(L, -3);
                }

                return 1;
            }
        }
        else if (wg->class == WG_PIECE)
        {
            struct wg_piece_internal* const piece = (struct wg_piece_internal* const)wg;

            if (strcmp(key, "zone") == 0)
            {
                lua_getglobal(L, "widgets");
                lua_pushlightuserdata(L, get_internal((struct wg_base*) piece->zone));
                lua_gettable(L, -2);

                return 1;
            }
        }
    }

    if (wg->jumptable.base->index)
    {
        const int output = wg->jumptable.base->index(L);

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
static int newindex(lua_State* L)
{
    struct wg_base_internal* const wg = (struct wg_base_internal*)luaL_checkudata(L, -3, "widget_mt");

    if (wg->jumptable.base->newindex)
    {
        const int output = wg->jumptable.base->newindex(L);

        if (output >= 0)
            return output;
    }

    lua_getfenv(L, -3);
    lua_replace(L, -4);

    lua_settable(L, -3);

    return 0;
}

/*********************************************/
/*              Widgets Methods              */
/*********************************************/

// TODO: Add type mask
static bool widgets_class_mask[WG_CLASS_CNT];

static int widgets_setmask(lua_State* L)
{
    for(enum wg_class class = 0; class < WG_CLASS_CNT; class++)
		widgets_class_mask[class] = lua_toboolean(L,class -WG_CLASS_CNT);

    lua_pop(L, WG_CLASS_CNT);

    return 0;
}

static int widgets_iter(lua_State* L)
{
    if (!lua_isfunction(L, -1))
        return 0;

    lua_getglobal(L, "widgets");

    struct wg_base_internal* wg, *wg_next;

    for(size_t class = 0; class < WG_CLASS_CNT; class++)
        if(widgets_class_mask[class])
			for (wg = queue_head[class]; wg; wg = wg_next)
			{
				wg_next = wg->next;

				lua_pushvalue(L, -2);
				lua_pushlightuserdata(L, wg);
				lua_gettable(L, -3);

				lua_call(L, 1, 0);
			}

    lua_pop(L, 2);

    return 0;
}

static int widgets_filter(lua_State* L)
{
    if (!lua_isfunction(L, -1))
        return 0;

    lua_createtable(L, 0, 0);

    lua_getglobal(L, "widgets");

    struct wg_base_internal* wg, * wg_next;

    for (size_t class = 0; class < WG_CLASS_CNT; class++)
        if (widgets_class_mask[class])
			for (wg = queue_head[class]; wg; wg = wg_next)
			{
				wg_next = wg->next;

				lua_pushvalue(L, -3);
				lua_pushlightuserdata(L, wg);
				lua_gettable(L, -3);

				lua_call(L, 1,1);

				if (!lua_toboolean(L, -1))
				{
					lua_pop(L, 1);
					continue;
				}

				lua_pop(L, 1);

				const size_t len = lua_objlen(L, -2);

				lua_pushinteger(L, len + 1);
				lua_pushlightuserdata(L, wg);
				lua_gettable(L, -3);

				lua_settable(L, -4);
			}

    lua_pop(L, 1);
    lua_remove(L, -2);

    return 1;
}

static int widgets_remove(lua_State* L)
{
    struct wg_base_internal* const wg = (struct wg_base_internal*)luaL_checkudata(L, 1, "widget_mt");

    lua_pushlightuserdata(L, wg);
    lua_pushnil(L);
    lua_settable(L, -3);

    queue_pop(wg);
    prevent_stale_pointers(wg);

    return 0;
}

static void widgets_init()
{
    // Make a weak global table to contain the widgets
    // And some functions for manipulating them
    lua_newtable(lua_state);
    lua_newtable(lua_state);

    lua_pushvalue(lua_state, -1);
    lua_setfield(lua_state, -2, "__index");

    lua_pushcfunction(lua_state, widgets_iter);
    lua_setfield(lua_state, -2, "iter");

    lua_pushcfunction(lua_state, widgets_filter);
    lua_setfield(lua_state, -2, "filter");

    lua_pushcfunction(lua_state, widgets_setmask);
    lua_setfield(lua_state, -2, "mask");

    lua_pushcfunction(lua_state, widgets_remove);
    lua_setfield(lua_state, -2, "remove");

    // This makes widgets a weak table
    if (1)
    {
        lua_pushstring(lua_state, "vk");
        lua_setfield(lua_state, -2, "__mode");
    }

    lua_setmetatable(lua_state, -2);
    lua_setglobal(lua_state, "widgets");

    for(size_t class = 0; class < WG_CLASS_CNT; class++)
		widgets_class_mask[class] = true;
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

// Initalize the Widget Engine
void widget_engine_init()
{
    // Set empty pointers to NULL
    for (size_t class = 0; class < WG_CLASS_CNT; class++)
    {
        queue_head[class] = NULL;
        queue_tail[class] = NULL;
    }

    current_drop = NULL;
    current_hover = NULL;
    last_click = NULL;

    // Arbitarly set snap to something nonzero
    snap_offset_x = 5;
    snap_offset_y = 5;

    onscreen_shader_init();
    offscreen_shader_init();

    style_init();

    // Camera
    keyframe_default(get_keyframe(&camera));
    tweener_init(&camera);
    lua_pushcfunction(lua_state, camera_push);
    lua_setglobal(lua_state, "camera_push");

    lua_pushcfunction(lua_state, camera_set);
    lua_setglobal(lua_state, "camera_set");

    //
    zone_and_piece_init();

    //
    widgets_init();

    lua_register(lua_state, "manual_move", manual_move);
        
    // Make the widget meta table
    luaL_newmetatable(lua_state, "widget_mt");

    lua_pushcfunction(lua_state, index);
    lua_setfield(lua_state, -2, "__index");

    lua_pushcfunction(lua_state, newindex);
    lua_setfield(lua_state, -2, "__newindex");

    lua_pushcfunction(lua_state, gc);
    lua_setfield(lua_state, -2, "__gc");

    lua_pop(lua_state, 1);
}

static struct wg_base_internal* wg_alloc(enum wg_class class, size_t size)
{
    struct wg_base_internal* const widget = lua_newuserdata(lua_state, size + sizeof(struct wg_header));

    if (!widget)
        return NULL;

    *widget = (struct wg_base_internal)
    {
        .class = class,
        .next = NULL,
        .previous = queue_tail[class],
    };

    keyframe_default((struct keyframe* const)get_keyframe(widget));

    switch (class)
    {
    case WG_HUD:
        widget->c = 0;
        break;

    case WG_PIECE:
    case WG_ZONE:
        widget->c = 1;
        break;
    }

    // Set metatable
    luaL_getmetatable(lua_state, "widget_mt");
    lua_setmetatable(lua_state, -2);

    // Add to "widgets" global
    lua_getglobal(lua_state, "widgets");
    lua_pushlightuserdata(lua_state, widget);
    lua_pushvalue(lua_state, -3);

    lua_settable(lua_state, -3);
    lua_pop(lua_state, 1);

    // Wire the widget into the queue
    if (queue_tail[class])
        queue_tail[class]->next = widget;
    else
        queue_head[class] = widget;

    queue_tail[class] = widget;
    
    if (LUA_TTABLE == lua_type(lua_state, -2))
    {
        // Process keyframes
        lua_getkeyframe(-2, (struct keyframe* const)downcast(widget));

        // Read Width
        lua_getfield(lua_state, -2, "width");

        if (lua_isnumber(lua_state, -1))
            widget->half_width = 0.5 * luaL_checknumber(lua_state, -1);

        // Read Height
        lua_getfield(lua_state, -3, "height");

        if (lua_isnumber(lua_state, -1))
            widget->half_height = 0.5 * luaL_checknumber(lua_state, -1);

        lua_pop(lua_state, 2);

        // Clean up some keys from the table
        lua_cleankeyframe(-2);

        lua_pushnil(lua_state);
        lua_setfield(lua_state, -3, "height");
        lua_pushnil(lua_state);
        lua_setfield(lua_state, -3, "width");

        // Set fenv
        lua_pushvalue(lua_state, -2);
        lua_setfenv(lua_state, -2);
    }
    else
    {
        lua_newtable(lua_state);
        lua_pushvalue(lua_state, -1);
        lua_setfenv(lua_state, -3);
    }

    tweener_init(widget);

    return widget;
}

struct wg_zone* wg_alloc_zone(size_t size, struct wg_jumptable_zone* jumptable)
{
    struct wg_zone_internal* wg = (struct wg_zone_internal*) wg_alloc(WG_ZONE, size);

    wg->draggable = false;
    wg->snappable = false;
    wg->jumptable.zone = (struct wg_jumptable_zone*)jumptable;

    wg->valid_move = false;
    wg->highlighted = false;
    wg->nominated = false;

    wg->used = 0;
    wg->allocated = 0;
    wg->pieces = NULL;

    return (struct wg_zone*)downcast((struct wg_base_internal*) wg);
}

struct wg_piece* wg_alloc_piece(size_t size, struct wg_jumptable_piece* jumptable)
{
    struct wg_piece_internal* wg = (struct wg_piece_internal*)wg_alloc(WG_PIECE, size);

    wg->draggable = true;
    wg->snappable = false;
    wg->jumptable.piece = (struct wg_jumptable_piece*)jumptable;

    wg->zone = NULL;

    return (struct wg_piece*)downcast((struct wg_base_internal*)wg);
}

struct wg_hud* wg_alloc_hud(size_t size, struct wg_jumptable_hud* jumptable)
{
    struct wg_hud_internal* wg = (struct wg_hud_internal*)wg_alloc(WG_HUD, size);

    wg->draggable = false;
    wg->snappable = false;
    wg->jumptable.hud = jumptable;

    wg->hud_state = HUD_IDLE;
    wg->pallet = &primary_pallet;

    return (struct wg_hud*)downcast((struct wg_base_internal*)wg);
}
