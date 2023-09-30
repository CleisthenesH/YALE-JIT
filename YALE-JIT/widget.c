// Copyright 2023 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

//#define WIDGET_DEBUG_DRAW

#include "widget.h"
#include "thread_pool.h"
#include "resource_manager.h"

#include <allegro5/allegro.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_opengl.h>

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

struct wg_header
{
	enum wg_type type;

	union {
		struct wg_jumptable_base* base;
		struct wg_jumptable_zone* zone;
		struct wg_jumptable_piece* piece;
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
/*             Widget Callbacks              */
/*********************************************/

// Callback macros assumes widget's type is (struct wg_base_internal*).
// Will also trigger a too many parameters error, but never actually called the function.

#define call_engine(widget,method) \
    do{ \
        if((widget)->type == WG_BASE) \
            if ((widget)->jumptable.base-> ## method) \
                (widget)->jumptable.base-> ## method(downcast(widget)); \
        if((widget)->type == WG_ZONE) \
            if ((widget)->jumptable.zone-> ## method) \
                (widget)->jumptable.zone-> ## method(downcast(widget)); \
        if((widget)->type == WG_PIECE) \
            if ((widget)->jumptable.piece-> ## method) \
                (widget)->jumptable.piece-> ## method(downcast(widget)); \
    }while(0);

#define call_engine_2(widget,method,object) \
    do{ \
        if((widget)->type == WG_BASE) \
            if ((widget)->jumptable.base-> ## method) \
                (widget)->jumptable.base-> ## method(downcast(widget),downcast(object)); \
        if((widget)->type == WG_ZONE) \
            if ((widget)->jumptable.zone-> ## method) \
                (widget)->jumptable.zone-> ## method(downcast(widget),downcast(object)); \
        if((widget)->type == WG_PIECE) \
            if ((widget)->jumptable.piece-> ## method) \
                (widget)->jumptable.piece-> ## method(downcast(widget),downcast(object)); \
    }while(0);

static void call_lua(struct wg_base_internal* const wg, const char* key, struct wg_base_internal* const obj)
{
    lua_getglobal(lua_state, "widgets");
    lua_pushlightuserdata(lua_state, wg);
    lua_gettable(lua_state, -2);

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

#define call(widget,method) \
    do{ \
        call_lua(widget, #method, NULL);\
        call_engine(widget,method);\
    }while(0); 

#define call_2(widget,method,obj) \
    do{ \
        call_lua(widget, #method, obj);\
        call_engine_2(widget, method, obj);\
     }while(0); 

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

    keyframe_copy(wg->tweener.keypoints,get_keyframe(wg));
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

    keyframe_blend(get_keyframe(wg), wg->tweener.keypoints+1, wg->tweener.keypoints,blend);
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

        struct keyframe* memsafe_hande = realloc(wg->tweener.keypoints, new_cnt *  sizeof(struct keyframe));

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
    const double blend_t = camera.t * blend;

    al_build_transform(&buffer,
        blend_x, blend_y,
        blend_sx, blend_sy,
        blend_t);

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
    ENGINE_STATE_LOCKED,                // The widget engine has been locked.
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
       "Locked"
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
/*          Widget Queue Methods             */
/*********************************************/

static struct wg_base_internal* queue_head;
static struct wg_base_internal* queue_tail;

// Pop a widget out of the engine
static void queue_pop(struct wg_base_internal* const widget)
{
    if (widget->next)
        widget->next->previous = widget->previous;
    else
        queue_tail = widget->previous;

    if (widget->previous)
        widget->previous->next = widget->next;
    else
        queue_head = widget->next;
}

// Insert the first widget behind the second
static void queue_insert(struct wg_base_internal* mover, struct wg_base_internal* target)
{
    // Assumes mover is outside the list.
    // I.e. that mover's next and previous are null.
    mover->next = target;

    // If the second is null append the first to the head
    if (target)
    {
        if (target->previous)
            target->previous->next = mover;
        else
            queue_head = mover;

        target->previous = mover;
    }
    else
    {
        mover->previous = queue_tail;

        if (queue_tail)
            queue_tail->next = mover;
        else
            queue_head = mover;

        queue_tail = mover;
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

    for (struct wg_base_internal* wg = queue_head; wg; wg = wg->next, picker_index++)
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

        call_engine(wg, mask);
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

    struct wg_base_internal* widget = queue_head;

    while (index-- > 1 && widget)
        widget = widget->next;

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
    if (current_hover == ptr)
    {
        call(ptr, hover_end);

        widget_engine_state = ENGINE_STATE_IDLE;
        current_hover = NULL;
    }

    if (last_click == ptr)
    {
        call(ptr, click_off)

            last_click = NULL;
    }

    if (current_drop == ptr)
    {
        call(ptr, drag_end_no_drop)

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

    //material_apply(NULL);
    glDisable(GL_STENCIL_TEST);

    if (wg->type == WG_BASE)
        wg->jumptable.base->draw((const struct wg_base* const) downcast(wg));
    else if (wg->type == WG_ZONE)
        wg->jumptable.zone->draw((const struct wg_base* const)downcast(wg));
    else if (wg->type == WG_PIECE)
        wg->jumptable.piece->draw((const struct wg_base* const)downcast(wg));

#ifdef WIDGET_DEBUG_DRAW
    al_draw_textf(debug_font, al_map_rgb_f(0, 1, 0), 10, 10, ALLEGRO_ALIGN_LEFT,
        "Self: %p, Prev: %p, Next: %p",
        wg, wg->previous, wg->next);
    //material_apply(NULL);
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
    for (struct wg_base_internal* widget = queue_head; widget; widget = widget->next)
        draw_widget((struct wg_base_internal*) widget);

    if (hide_hover)
    {
        queue_insert(current_hover, current_hover->next);
        draw_widget(current_hover);
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
    if (widget_engine_state == ENGINE_STATE_LOCKED)
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

        buffer.dx = mouse_x - drag_offset_x;
        buffer.dy = mouse_y - drag_offset_y;

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
    struct work_queue* work_queue = work_queue_create();

    // Since the update method doesn't change maybe we should have a static queue?
    if (widget_engine_state != ENGINE_STATE_LOCKED)
        for (struct wg_base_internal* widget = queue_head; widget; widget = widget->next)
        {
            work_queue_push(work_queue,tweener_blend,widget);

            if(widget->type == WG_BASE && widget->jumptable.base->update)
                work_queue_push(work_queue, widget->jumptable.base->update, widget);
        }

    return work_queue;
}

// Handle events by calling all widgets that have a event handler.
void widget_engine_event_handler()
{
    // TODO: Incorperate to the threadpool?
    if (widget_engine_state != ENGINE_STATE_LOCKED)
        for (struct wg_base_internal* widget = queue_head; widget; widget = widget->next)
            call_engine(widget, event_handler);

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
        if (current_event.mouse.button != 1)
            break;

        if (widget_engine_state == ENGINE_STATE_EMPTY_DRAG)
        {
            widget_engine_state = ENGINE_STATE_IDLE;
            break;
        }

        // Split up just in case empty_drag and current_hover desync
        if (!current_hover)
            break;

        switch (widget_engine_state)
        {
        case ENGINE_STATE_PRE_DRAG_THRESHOLD:
            call(current_hover, left_click);
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
            if (current_drop)
            {
                call_2(current_drop, drag_end_drop, current_hover);
                call(current_hover, drag_end_no_drop);
            }
            else
                call(current_hover, drag_end_no_drop);

            current_drop = NULL;
            break;
        }

        widget_engine_state = current_hover ? ENGINE_STATE_HOVER : ENGINE_STATE_IDLE;
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
    }
    
}

/*********************************************/
/*               LUA interface               */
/*********************************************/

// General widget garbage collection
static int gc(lua_State* L)
{
    struct wg_base_internal* const wg = (struct wg_base_internal*)luaL_checkudata(L, 1, "wg_mt");

    if (wg->jumptable.base->gc)
        wg->jumptable.base->gc(wg);

    call_engine(wg, gc);
    queue_pop(wg);

    // Make sure we don't get stale pointers
    prevent_stale_pointers(wg);

    return 0;
}

// General widget index method
static int index(lua_State* L)
{
    struct wg_base_internal* const wg = (struct wg_base_internal*)luaL_checkudata(L, -2, "widget_mt");

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
/*               Widget Style                */
/*********************************************/

struct widget_pallet primary_pallet, secondary_pallet;
ALLEGRO_FONT* primary_font;

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

        .activated = al_map_rgb(0,0,0),
        .deactivated = al_map_rgb(128,128,128)
    };

    secondary_pallet = primary_pallet;

    primary_font = resource_manager_font(FONT_ID_SHINYPEABERRY);
}

/*********************************************/
/*           Widget Engine Inits             */
/*********************************************/

// Initalize the Widget Engine
void widget_engine_init()
{
    // Set empty pointers to NULL
    queue_head = NULL;
    queue_tail = NULL;

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

    // Make a weak global table to contain the widgets
    // And some functions for manipulating them
    lua_newtable(lua_state);
    lua_newtable(lua_state);

    lua_pushvalue(lua_state, -1);
    lua_setfield(lua_state, -2, "__index");

    lua_pushstring(lua_state, "vk");
    lua_setfield(lua_state, -2, "__mode");

    lua_setmetatable(lua_state, -2);
    lua_setglobal(lua_state, "widgets");
        
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

struct wg_base* wg_alloc(enum wg_type type, size_t size, struct wg_jumptable_base* jumptable)
{
    struct wg_base_internal* const widget = lua_newuserdata(lua_state, size + sizeof(struct wg_header));

    if (!widget)
        return NULL;

    *widget = (struct wg_base_internal)
    {
        .type = type,
        .next = NULL,
        .previous = queue_tail,
    };

    keyframe_default((struct keyframe* const)get_keyframe(widget));

    switch (type)
    {
    case WG_BASE:
        widget->draggable = false;
        widget->snappable = false;
        widget->jumptable.base = jumptable;
        widget->c = 0;

        break;

    case WG_ZONE:
        widget->draggable = false;
        widget->snappable = true;
        widget->jumptable.zone = (struct wg_jumptable_zone*) jumptable;
        widget->c = 1;

        break;

    case WG_PIECE:
        widget->draggable = true;
        widget->snappable = true;
        widget->jumptable.piece = (struct wg_jumptable_piece*) jumptable;
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
    if (queue_tail)
        queue_tail->next = widget;
    else
        queue_head = widget;

    queue_tail = widget;
    
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
        lua_setfenv(lua_state, -2);
    }

    tweener_init(widget);

    return downcast(widget);
}
