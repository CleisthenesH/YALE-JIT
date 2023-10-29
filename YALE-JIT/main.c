// Copyright 2023 Kieran W Harvie. All rights reserved.
// Use of this source code is governed by an MIT-style
// license that can be found in the LICENSE file.

#include <stdio.h>
#include <math.h>

#include <allegro5/allegro.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_image.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <allegro5/allegro_color.h>
#include <allegro5/allegro_opengl.h>

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <luajit.h>

#define EASY_BACKGROUND
#define EASY_BOARDER

// Thread Pool includes
#include "thread_pool.h"
void thread_pool_init(size_t);
void thread_pool_destroy();


// Widget Interface includes
void widget_engine_init();
void widget_engine_draw();
struct work_queue* widget_engine_widget_work();
void widget_engine_update();
void widget_engine_event_handler();
void widget_interface_shader_predraw();

// Miscellaneous Lua Interfaces
void lua_openL_misc(lua_State*);

// Resource Manager includes
void resource_manager_init();

// Scheduler includes
ALLEGRO_EVENT_SOURCE* scheduler_init();
void scheduler_generate_events();

// Static variable declaration
static ALLEGRO_DISPLAY* display;
static ALLEGRO_EVENT_QUEUE* main_event_queue;
struct thread_pool* thread_pool;
static bool do_exit;

// A simple background for testing
#ifdef EASY_BACKGROUND
static ALLEGRO_BITMAP* easy_background;
const double easy_background_speed = 0;
#include "material.h"
#endif

// A simple FPS Monitor
#ifdef EASY_FPS
static double last_render_timestamp;
#endif

// These varitables are keep seperate from their normal use so they don't change during processing  
// Also allows custom mapping on things like mouse or add a slow down factor for timestamps
double mouse_x, mouse_y;
ALLEGRO_EVENT current_event;
double future_timestamp;
double current_timestamp;
double residual_timestamp;
double delta_timestamp;

ALLEGRO_TRANSFORM identity_transform;
ALLEGRO_FONT* debug_font;
lua_State* lua_state;

// Initalze the lua enviroment.
static inline int lua_init()
{
    lua_state = luaL_newstate();

    if (!lua_state)
        return 0;

    luaL_openlibs(lua_state);

    return 1;
}

// Wraps the lua_dofile with some error reporting
static inline void lua_dofile_wrapper(const char* file_name)
{
    int error = luaL_dofile(lua_state, file_name);

    if (!error)
        return;

    // TODO: check that error codes are right since lua 5.1 had different codes than 5.4
    printf("While running file % s an error of type: \"", file_name);

    switch (error)
    {
    case LUA_ERRRUN:
        printf("Runtime");
        break;
    case LUA_ERRMEM:
        printf("Memmory Allocation");
        break;
    case LUA_ERRERR:
        printf("Message Handler");
        break;
    case LUA_ERRSYNTAX:
        printf("Syntax");
        break;
    case LUA_YIELD:
        printf("Unexpected Yeild");
        break;
    case LUA_ERRFILE:
        printf("File Related");
        break;
    default:
        printf("Unkown Type (%d)", error);
    }

    const char* error_message = luaL_checkstring(lua_state, -1);

    printf("\" occurred\n\t%s\n\n", error_message);
}

// Resolve and Run a bootfile based on main_lua_state
static inline void lua_boot_file()
{
    lua_getglobal(lua_state, "boot_file");

    if (lua_isstring(lua_state, -1))
    {
        const char* boot_file = luaL_checkstring(lua_state, -1);

        lua_dofile_wrapper(boot_file);

        lua_pushnil(lua_state);
        lua_setglobal(lua_state, "boot_file");
    }
    else
    {
        lua_dofile_wrapper("lua/boot.lua");
    }

    lua_pop(lua_state, 1);
}

// Config and create display
static inline void create_display()
{
    int adapter = ALLEGRO_DEFAULT_DISPLAY_ADAPTER;
    int display_flags = ALLEGRO_PROGRAMMABLE_PIPELINE | ALLEGRO_OPENGL;

    lua_getglobal(lua_state, "video_adapter");

    if (lua_isnumber(lua_state, -1))
    {
        adapter = luaL_checkint(lua_state, -1);

        if (adapter < 0 || adapter >= al_get_num_display_modes())
            adapter = ALLEGRO_DEFAULT_DISPLAY_ADAPTER;

        lua_pushnil(lua_state);
        lua_setglobal(lua_state, "video_adapter");
    }

    lua_getglobal(lua_state, "windowed");

    if (!lua_isnil(lua_state, -1))
    {
        lua_pushnil(lua_state);
        lua_setglobal(lua_state, "windowed");
    }
    else
        display_flags |= ALLEGRO_FULLSCREEN;

    lua_pop(lua_state, 2);
 
    al_set_new_display_flags(display_flags);

    al_set_new_display_option(ALLEGRO_DEPTH_SIZE, 32, ALLEGRO_SUGGEST);
    al_set_new_display_option(ALLEGRO_STENCIL_SIZE, 8, ALLEGRO_SUGGEST);
    al_set_new_display_option(ALLEGRO_SAMPLE_BUFFERS, 1, ALLEGRO_REQUIRE);
    al_set_new_display_option(ALLEGRO_SAMPLES, 16, ALLEGRO_REQUIRE);

   ALLEGRO_DISPLAY_MODE display_mode;

    al_set_new_display_adapter(adapter);
    al_get_display_mode(adapter, &display_mode);

    display = al_create_display(display_mode.width, display_mode.height);

    al_set_render_state(ALLEGRO_ALPHA_TEST, 1);
    al_set_render_state(ALLEGRO_ALPHA_FUNCTION, ALLEGRO_RENDER_NOT_EQUAL);
    al_set_render_state(ALLEGRO_ALPHA_TEST_VALUE, 0);

    if (!display) 
    {
        fprintf(stderr, "failed to create display!\n");
        return;
    }

#ifdef EASY_BACKGROUND
    easy_background = al_create_bitmap(al_get_display_width(display) + 100, al_get_display_height(display) + 100);

    al_set_target_bitmap(easy_background);

    // Keeping these here incase I change init call order
    // 
    //al_use_transform(&identity_transform);
    //al_use_shader(NULL);

    al_clear_to_color(al_color_name("aliceblue"));
    unsigned int i, j;

    const unsigned int height = al_get_display_height(display) / 10 + 10;
    const unsigned int width = al_get_display_width(display) / 10 + 10;

    for (i = 0; i <= width; i++)
        for (j = 0; j <= height; j++)
            if (i % 10 == 0 && j % 10 == 0)
                al_draw_circle(10 * i, 10 * j, 2, al_color_name("darkgray"), 2);
            else
                al_draw_circle(10 * i, 10 * j, 1, al_color_name("grey"), 0);

#endif

    al_set_target_bitmap(al_get_backbuffer(display));
}

// Create event queue and register sources
static inline void create_event_queue()
{
    main_event_queue = al_create_event_queue();

    al_register_event_source(main_event_queue, al_get_display_event_source(display));
    al_register_event_source(main_event_queue, al_get_mouse_event_source());
    al_register_event_source(main_event_queue, al_get_keyboard_event_source());
}

// Initalize the allegro enviroment.
static inline int allegro_init()
{
    // Initalize the base allegro
    if (!al_init()) {
        fprintf(stderr, "failed to initialize allegro!\n");
        return 0;
    }

    // Initalize the drawing primitive shapes
    if (!al_init_primitives_addon()) {
        fprintf(stderr, "failed to initialize allegro_primitives_addon!\n");
        return 0;
    }

    // Initalize the drawing bitmaps
    if (!al_init_image_addon()) {
        fprintf(stderr, "failed to initialize al_init_image_addon!\n");
        return 0;
    }

    // Initalize the fonts allegro
    if (!al_init_font_addon()) {
        fprintf(stderr, "failed to initialize al_init_font_addon!\n");
        return 0;
    }

    // Initalize the fonts file extension allegro
    if (!al_init_ttf_addon()) {
        fprintf(stderr, "failed to initialize al_init_ttf_addon!\n");
        return 0;
    }

    // Initalize the keyboard allegro
    if (!al_install_keyboard()) {
        fprintf(stderr, "failed to initialize al_install_keyboard!\n");
        return 0;
    }

    // Initalize the mouse allegro
    if (!al_install_mouse()) {
        fprintf(stderr, "failed to initialize al_install_mouse!\n");
        return 0;
    }

    create_display();
    create_event_queue();

    return 1;
}

// Initalize the global enviroment.
static inline void global_init()
{
    debug_font = al_create_builtin_font();
    al_identity_transform(&identity_transform);

    time_t time_holder;
    srand((unsigned)time(&(time_holder)));

    do_exit = false;
    current_timestamp = al_get_time();
    future_timestamp = current_timestamp;
    residual_timestamp = 0;
    delta_timestamp = 0;
}

// Process the current event.
static inline void process_event()
{
    switch (current_event.type)
    {
    case ALLEGRO_EVENT_DISPLAY_CLOSE:
        do_exit = true;
        return;

    case ALLEGRO_EVENT_KEY_CHAR:
        if (current_event.keyboard.modifiers & ALLEGRO_KEYMOD_CTRL &&
            current_event.keyboard.keycode == ALLEGRO_KEY_C)
        {
            do_exit = true;
            return;
        }
        break;

    case ALLEGRO_EVENT_MOUSE_AXES:
        mouse_x = current_event.mouse.x;
        mouse_y = current_event.mouse.y;

        break;

    case ALLEGRO_GET_EVENT_TYPE('T', 'I', 'M', 'E'):
        ((void (*)(void*)) current_event.user.data1)((void*)current_event.user.data2);

        return;
    }

    widget_engine_event_handler();
}

// Populate and run the thread pool. Doesn't wait for completion.
static inline void update_work_queue()
{
    // future_timestamp and current_time_stamp must be accurate upon calling this function.
    delta_timestamp = future_timestamp - current_timestamp;
    
	// Update tweeners
    struct work_queue* queue = widget_engine_widget_work();

	thread_pool_concatenate(queue);
    widget_engine_update();
    thread_pool_wait();
	
    current_timestamp = future_timestamp;
}

// All the drawing that can be done before waiting for widget update to complete.
static inline void predraw()
{
    // The residual time can be used for projection in drawing
    residual_timestamp = al_get_time() - future_timestamp;

    // Process predraw then wait
    al_set_target_bitmap(al_get_backbuffer(display));
    al_set_blender(ALLEGRO_ADD, ALLEGRO_ONE, ALLEGRO_INVERSE_ALPHA);
    al_set_render_state(ALLEGRO_ALPHA_TEST, 1);

    glStencilMask(0xFF);
    glClear(GL_COLOR_BUFFER_BIT | GL_STENCIL_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_STENCIL_TEST);
    al_reset_clipping_rectangle();

    widget_interface_shader_predraw();

#ifdef EASY_BACKGROUND
    al_use_transform(&identity_transform);
    material_apply(NULL);
    const double easy_background_offset = -fmod(easy_background_speed * current_timestamp, 100);
    al_draw_bitmap(easy_background, easy_background_offset, easy_background_offset, 0);
#endif
}

void main()
{
    // Init Lua first so we can read a config file to inform later inits
    lua_init();
    lua_dofile_wrapper("lua/config.lua");

    // Init the Allegro Environment
    allegro_init();
    thread_pool_init(8);
    global_init();

	// Init Systems, check dependency graph for order.
	resource_manager_init();
    al_register_event_source(main_event_queue, scheduler_init());

    // Miscellaneous Lua Interfaces
    lua_openL_misc(lua_state);
    
    // Init Widgets
    widget_engine_init();

    // Resolve and Read Boot File
    lua_boot_file();

    // Main loop
    while (!do_exit)
    {
        future_timestamp = al_get_time();

        if (future_timestamp - current_timestamp > 0.25)
            future_timestamp = current_timestamp + 0.25;

        scheduler_generate_events();

        if (al_peek_next_event(main_event_queue, &current_event)
            && current_event.any.timestamp <= future_timestamp)
        {
            future_timestamp = current_event.any.timestamp;

            update_work_queue();
            thread_pool_wait();

            process_event();
            al_drop_next_event(main_event_queue);

            continue;
        }

        update_work_queue();
        predraw();
        thread_pool_wait();
        widget_engine_draw();

#ifdef EASY_BOARDER
        al_use_transform(&identity_transform);
        material_apply(NULL);
        al_draw_rectangle(0, 0, 
            al_get_display_width(display), al_get_display_height(display),
            al_map_rgba(0,0,0,100), 10);
#endif

#ifdef EASY_FPS
        al_use_transform(&identity_transform);
        material_apply(NULL);
        al_draw_textf(debug_font, al_map_rgb_f(0, 1, 0), 0, 0, 0, "FPS:%lf  Timestamp:%lf", 1.0 / (current_timestamp - last_render_timestamp), current_timestamp);
        last_render_timestamp = current_timestamp;
#endif

        // Flip
        al_flip_display();
    }
}