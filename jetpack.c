#include <stdlib.h>

#include <jetpack_joyride_icons.h>
#include <furi.h>
#include <gui/gui.h>
#include <gui/icon_animation.h>
#include <input/input.h>

#include "includes/point.h"
#include "includes/barry.h"
#include "includes/scientist.h"
#include "includes/particle.h"
#include "includes/coin.h"
#include "includes/missile.h"
#include "includes/background_assets.h"

#include "includes/game_state.h"

#define TAG "Jetpack Joyride"

typedef enum {
    EventTypeTick,
    EventTypeKey,
} EventType;

typedef struct {
    EventType type;
    InputEvent input;
} GameEvent;

static void jetpack_game_state_init(GameState* const game_state) {
    UNUSED(game_state);
    BARRY barry;
    barry.gravity = 0;
    barry.point.x = 32 + 5;
    barry.point.y = 32;
    barry.isBoosting = false;

    GameSprites sprites;
    sprites.barry = icon_animation_alloc(&A_barry);
    sprites.barry_infill = &I_barry_infill;

    sprites.scientist_left = (&I_scientist_left);
    sprites.scientist_left_infill = (&I_scientist_left_infill);
    sprites.scientist_right = (&I_scientist_right);
    sprites.scientist_right_infill = (&I_scientist_right_infill);

    sprites.missile = icon_animation_alloc(&A_missile);
    sprites.missile_infill = &I_missile_infill;

    // sprites.bg[0] = &I_bg1;
    // sprites.bg[1] = &I_bg2;
    // sprites.bg[2] = &I_bg3;

    // for(int i = 0; i < 3; ++i) {
    //     sprites.bg_pos[i].x = i * 128;
    //     sprites.bg_pos[i].y = 0;
    // }

    icon_animation_start(sprites.barry);
    icon_animation_start(sprites.missile);

    game_state->barry = barry;
    game_state->points = 0;
    game_state->distance = 0;
    game_state->sprites = sprites;
    game_state->state = GameStateLife;

    memset(game_state->scientists, 0, sizeof(game_state->scientists));
    memset(game_state->coins, 0, sizeof(game_state->coins));
    memset(game_state->particles, 0, sizeof(game_state->particles));
}

static void jetpack_game_state_free(GameState* const game_state) {
    icon_animation_free(game_state->sprites.barry);
    icon_animation_free(game_state->sprites.missile);

    free(game_state);
}

static void jetpack_game_tick(GameState* const game_state) {
    if(game_state->state == GameStateGameOver) return;
    barry_tick(&game_state->barry);
    game_state_tick(game_state);
    coin_tick(game_state->coins, &game_state->barry, &game_state->points);
    particle_tick(game_state->particles, game_state->scientists, &game_state->points);
    scientist_tick(game_state->scientists);
    missile_tick(game_state->missiles, &game_state->barry, &game_state->state);

    background_assets_tick(game_state->bg_assets);

    if(game_state->distance % 64 == 0) {
        spawn_random_background_asset(game_state->bg_assets);
    }

    // for(int i = 0; i < 3; ++i) {
    //     game_state->sprites.bg_pos[i].x -= 1;
    //     if(game_state->sprites.bg_pos[i].x <= -128) {
    //         game_state->sprites.bg_pos[i].x = 128 * 2; // 2 other images are 128 px each
    //     }
    // }

    if((rand() % 100) < 1) {
        spawn_random_coin(game_state->coins);
    }

    if((rand() % 100) < 1) {
        spawn_random_missile(game_state->missiles);
    }

    spawn_random_scientist(game_state->scientists);

    if(game_state->barry.isBoosting) {
        spawn_random_particles(game_state->particles, &game_state->barry);
    }
}

static void jetpack_game_render_callback(Canvas* const canvas, void* ctx) {
    furi_assert(ctx);
    const GameState* game_state = ctx;
    furi_mutex_acquire(game_state->mutex, FuriWaitForever);

    if(game_state->state == GameStateLife) {
        // canvas_draw_box(canvas, 0, 0, 128, 32);

        // for(int i = 0; i < 3; ++i) {
        //     // Check if the image is within the screen's boundaries
        //     if(game_state->sprites.bg_pos[i].x >= -127 && game_state->sprites.bg_pos[i].x < 128) {
        //         canvas_draw_icon(
        //             canvas, game_state->sprites.bg_pos[i].x, 0, game_state->sprites.bg[i]);
        //     }
        // }

        canvas_set_bitmap_mode(canvas, false);

        draw_background_assets(game_state->bg_assets, canvas, game_state->distance);

        canvas_set_bitmap_mode(canvas, true);

        draw_coins(game_state->coins, canvas);
        draw_scientists(game_state->scientists, canvas, &game_state->sprites);
        draw_particles(game_state->particles, canvas);
        draw_missiles(game_state->missiles, canvas, &game_state->sprites);

        draw_barry(&game_state->barry, canvas, &game_state->sprites);

        canvas_set_color(canvas, ColorBlack);
        canvas_set_font(canvas, FontSecondary);
        char buffer[12];
        snprintf(buffer, sizeof(buffer), "Dist: %u", game_state->distance);
        canvas_draw_str_aligned(canvas, 123, 12, AlignRight, AlignBottom, buffer);

        snprintf(buffer, sizeof(buffer), "Score: %u", game_state->points);
        canvas_draw_str_aligned(canvas, 5, 12, AlignLeft, AlignBottom, buffer);
    }

    if(game_state->state == GameStateGameOver) {
        // Show highscore

        char buffer[12];
        snprintf(buffer, sizeof(buffer), "Dist: %u", game_state->distance);
        canvas_draw_str_aligned(canvas, 123, 12, AlignRight, AlignBottom, buffer);

        snprintf(buffer, sizeof(buffer), "Score: %u", game_state->points);
        canvas_draw_str_aligned(canvas, 5, 12, AlignLeft, AlignBottom, buffer);

        canvas_draw_str_aligned(canvas, 64, 32, AlignCenter, AlignCenter, "boom.");

        // if(furi_timer_is_running(game_state->timer)) {
        //     furi_timer_start(game_state->timer, 0);
        // }
    }

    canvas_draw_frame(canvas, 0, 0, 128, 64);

    furi_mutex_release(game_state->mutex);
}

static void jetpack_game_input_callback(InputEvent* input_event, FuriMessageQueue* event_queue) {
    furi_assert(event_queue);

    GameEvent event = {.type = EventTypeKey, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void jetpack_game_update_timer_callback(FuriMessageQueue* event_queue) {
    furi_assert(event_queue);

    GameEvent event = {.type = EventTypeTick};
    furi_message_queue_put(event_queue, &event, 0);
}

int32_t jetpack_game_app(void* p) {
    UNUSED(p);
    int32_t return_code = 0;

    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(GameEvent));

    GameState* game_state = malloc(sizeof(GameState));
    jetpack_game_state_init(game_state);

    game_state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    if(!game_state->mutex) {
        FURI_LOG_E(TAG, "cannot create mutex\r\n");
        return_code = 255;
        goto free_and_exit;
    }

    // Set system callbacks
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, jetpack_game_render_callback, game_state);
    view_port_input_callback_set(view_port, jetpack_game_input_callback, event_queue);

    FuriTimer* timer =
        furi_timer_alloc(jetpack_game_update_timer_callback, FuriTimerTypePeriodic, event_queue);
    furi_timer_start(timer, furi_kernel_get_tick_frequency() / 25);

    game_state->timer = timer;

    // Open GUI and register view_port
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    GameEvent event;
    for(bool processing = true; processing;) {
        FuriStatus event_status = furi_message_queue_get(event_queue, &event, 100);
        furi_mutex_acquire(game_state->mutex, FuriWaitForever);

        if(event_status == FuriStatusOk) {
            // press events
            if(event.type == EventTypeKey) {
                if(event.input.type == InputTypeRelease && event.input.key == InputKeyOk) {
                    game_state->barry.isBoosting = false;
                }

                if(event.input.type == InputTypePress) {
                    switch(event.input.key) {
                    case InputKeyUp:
                        break;
                    case InputKeyDown:
                        break;
                    case InputKeyRight:
                        break;
                    case InputKeyLeft:
                        break;
                    case InputKeyOk:
                        if(game_state->state == GameStateGameOver) {
                            jetpack_game_state_init(game_state);
                        }

                        if(game_state->state == GameStateLife) {
                            // Do something
                            game_state->barry.isBoosting = true;
                        }

                        break;
                    case InputKeyBack:
                        processing = false;
                        break;
                    default:
                        break;
                    }
                }
            } else if(event.type == EventTypeTick) {
                jetpack_game_tick(game_state);
            }
        }

        view_port_update(view_port);
        furi_mutex_release(game_state->mutex);
    }

    furi_timer_free(timer);
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_mutex_free(game_state->mutex);

free_and_exit:
    jetpack_game_state_free(game_state);
    furi_message_queue_free(event_queue);

    return return_code;
}