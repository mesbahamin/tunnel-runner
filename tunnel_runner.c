#include <assert.h>
#include <inttypes.h>
#include <SDL.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>

#define TR_PI32 3.14159265359f

#define TR_SCREEN_WIDTH 640
#define TR_SCREEN_HEIGHT 480
#define TR_TEX_WIDTH 256
#define TR_TEX_HEIGHT 256
#define TR_BYTES_PER_PIXEL 4
#define TR_MAX_CONTROLLERS 4
#define TR_MOVEMENT_SPEED 5
#define TR_CONTROLLER_STICK_MAX 32770
#define TR_CONTROLLER_STICK_MIN -32770

// TODO: Should time be stored in a double of seconds?
#define TR_SECOND 1000
#define TR_FPS 60
#define TR_MS_PER_FRAME (TR_SECOND / TR_FPS)
#define TR_UPDATES_PER_SECOND 120
#define TR_MS_PER_UPDATE (TR_SECOND / TR_UPDATES_PER_SECOND)

#define TR_LOG_ERR(message, ...) fprintf(stderr, (message), ##__VA_ARGS__)

#ifdef TR_LOGLEVEL_DEBUG
#define TR_LOG_DBG(message, ...) printf((message), ##__VA_ARGS__)
#else
#define TR_LOG_DBG(message, ...)
#endif

#ifdef TR_LOGLEVEL_DEBUG_FRAME
#define TR_LOG_FRM(message, ...) TR_LOG_DBG((message), ##__VA_ARGS__)
#else
#define TR_LOG_FRM(message, ...)
#endif

enum Color
{
    COLOR_GREEN,
    COLOR_RED,
    COLOR_BLUE,
    COLOR_YELLOW,
    COLOR_MAGENTA,
    COLOR_CYAN,
    COLOR_WHITE
};

struct SDLOffscreenBuffer
{
    // pixels are always 32-bits wide. Memory order: BB GG RR XX.
    SDL_Texture *texture;
    void *memory;
    uint32_t width;
    uint32_t height;
    int32_t pitch;
};

struct SDLWindowDimension
{
    int32_t width;
    int32_t height;
};

struct TransformData
{
    int32_t width;
    int32_t height;
    int32_t **distance_table;
    int32_t **angle_table;
    int32_t look_shift_x;
    int32_t look_shift_y;
};

static struct SDLOffscreenBuffer global_back_buffer;
static SDL_GameController *controller_handles[TR_MAX_CONTROLLERS];
static SDL_Haptic *rumble_handles[TR_MAX_CONTROLLERS];
static struct TransformData transform;


uint64_t
get_current_time_ms(void)
{
    struct timespec current;
    clock_gettime(CLOCK_MONOTONIC, &current);
    uint64_t milliseconds = ((current.tv_sec * 1000000000) + current.tv_nsec) / 1000000;
    return milliseconds;
}


void
render_texture(
        struct SDLOffscreenBuffer buffer,
        uint32_t texture[TR_TEX_HEIGHT][TR_TEX_WIDTH],
        int32_t x_offset,
        int32_t y_offset,
        enum Color color_choice)
{
    uint8_t *row = (uint8_t *)buffer.memory;

    for (uint32_t y = 0; y < buffer.height; ++y)
    {
        uint32_t *pixel = (uint32_t *)row;
        for (uint32_t x = 0; x < buffer.width; ++x)
        {
            uint8_t color = texture[
                (uint32_t)(y + y_offset) % TR_TEX_HEIGHT
            ]
            [
                (uint32_t)(x + x_offset) % TR_TEX_WIDTH
            ];
            uint32_t red = color << 16;
            uint32_t green = color << 8;
            uint32_t blue = color;

            switch(color_choice)
            {
                case COLOR_GREEN:
                {
                    *pixel++ = green;
                } break;
                case COLOR_RED:
                {
                    *pixel++ = red;
                } break;
                case COLOR_BLUE:
                {
                    *pixel++ = blue;
                } break;
                case COLOR_YELLOW:
                {
                    *pixel++ = red | green;
                } break;
                case COLOR_MAGENTA:
                {
                    *pixel++ = red | blue;
                } break;
                case COLOR_CYAN:
                {
                    *pixel++ = blue | green;
                } break;
                case COLOR_WHITE:
                {
                    *pixel++ = red | green | blue;
                } break;
                default:
                {
                    TR_LOG_ERR("Invalid color enum value: %d\n", color_choice);
                    *pixel++ = red | green | blue;
                } break;
            }
        }

        row += buffer.pitch;
    }
}


void
render_tunnel(
        struct SDLOffscreenBuffer buffer,
        uint32_t texture[TR_TEX_HEIGHT][TR_TEX_WIDTH],
        int32_t rotation_offset,
        int32_t translation_offset,
        enum Color color_choice)
{
    uint8_t *row = (uint8_t *)buffer.memory;

    for (uint32_t y = 0; y < buffer.height; ++y)
    {
        uint32_t *pixel = (uint32_t *)row;
        for (uint32_t x = 0; x < buffer.width; ++x)
        {
            uint32_t texel_y = (uint32_t)(transform.distance_table[y + transform.look_shift_y][x + transform.look_shift_x] + translation_offset) % TR_TEX_HEIGHT;
            uint32_t texel_x = (uint32_t)(transform.angle_table[y + transform.look_shift_y][x + transform.look_shift_x] + rotation_offset) % TR_TEX_WIDTH;

            assert(texel_x >= 0 && texel_x < TR_TEX_WIDTH);
            assert(texel_y >= 0 && texel_y < TR_TEX_HEIGHT);

            uint8_t texel = texture[texel_y][texel_x];

            uint32_t red = texel << 16;
            uint32_t green = texel << 8;
            uint32_t blue = texel;

            switch(color_choice)
            {
                case COLOR_GREEN:
                {
                    *pixel++ = green;
                } break;
                case COLOR_RED:
                {
                    *pixel++ = red;
                } break;
                case COLOR_BLUE:
                {
                    *pixel++ = blue;
                } break;
                case COLOR_YELLOW:
                {
                    *pixel++ = red | green;
                } break;
                case COLOR_MAGENTA:
                {
                    *pixel++ = red | blue;
                } break;
                case COLOR_CYAN:
                {
                    *pixel++ = blue | green;
                } break;
                case COLOR_WHITE:
                {
                    *pixel++ = red | green | blue;
                } break;
                default:
                {
                    *pixel++ = red | green | blue;
                    TR_LOG_ERR("Invalid color enum value: %d\n", color_choice);
                } break;
            }
        }
        row += global_back_buffer.pitch;
    }
}


struct SDLWindowDimension
sdl_get_window_dimension(SDL_Window *window)
{
    int w = 0;
    int h = 0;
    SDL_GetWindowSize(window, &w, &h);
    struct SDLWindowDimension result = { .width = w, .height = h };
    return result;
}


void
sdl_resize_texture(struct SDLOffscreenBuffer *buffer, SDL_Renderer *renderer, int32_t window_width, int32_t window_height)
{
    if (buffer->memory)
    {
        free(buffer->memory);
    }

    if (buffer->texture)
    {
        SDL_DestroyTexture(buffer->texture);
    }

    if (transform.distance_table)
    {
        for (int32_t y = 0; y < transform.height; ++y)
        {
            free(transform.distance_table[y]);
        }
        free(transform.distance_table);
    }
    if (transform.angle_table)
    {
        for (int32_t y = 0; y < transform.height; ++y)
        {
            free(transform.angle_table[y]);
        }
        free(transform.angle_table);
    }

    buffer->texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            window_width, window_height);

    buffer->width = window_width;
    buffer->height = window_height;
    buffer->pitch = window_width * TR_BYTES_PER_PIXEL;

    buffer->memory = malloc(window_width * window_height * TR_BYTES_PER_PIXEL);

    transform.width = 2 * window_width;
    transform.height = 2 * window_height;
    transform.look_shift_x = window_width / 2;
    transform.look_shift_y = window_height / 2;
    transform.distance_table = malloc(transform.height * sizeof(int32_t *));
    transform.angle_table = malloc(transform.height * sizeof(int32_t *));

    for (int32_t y = 0; y < transform.height; ++y)
    {
        transform.distance_table[y] = malloc(transform.width * sizeof(int32_t));
        transform.angle_table[y] = malloc(transform.width * sizeof(int32_t));
    }

    // Make distance and angle transformation tables
    for (int32_t y = 0; y < transform.height; ++y)
    {
        for (int32_t x = 0; x < transform.width; ++x)
        {
            int32_t dist_from_center_x = x - window_width;
            int32_t dist_from_center_y = y - window_height;
            float dist_from_center = sqrtf((float)(dist_from_center_x * dist_from_center_x + dist_from_center_y * dist_from_center_y));
            float angle_from_positive_x_axis = atan2f((float)dist_from_center_y, (float)dist_from_center_x) / TR_PI32;

            float ratio = 32.0f;
            transform.distance_table[y][x] = (int32_t)(ratio * TR_TEX_HEIGHT / dist_from_center) % TR_TEX_HEIGHT;
            transform.angle_table[y][x] = (int32_t)(0.5f * TR_TEX_WIDTH * angle_from_positive_x_axis);
        }
    }
}


void
sdl_update_window(SDL_Renderer *renderer, struct SDLOffscreenBuffer buffer)
{
    if (SDL_UpdateTexture(buffer.texture, 0, buffer.memory, buffer.pitch))
    {
        TR_LOG_ERR("SDL_UpdateTexture failed: %s\n", SDL_GetError());
    }

    SDL_RenderCopy(renderer, buffer.texture, 0, 0);
    SDL_RenderPresent(renderer);
}


bool
handle_event(SDL_Event *event)
{
    bool should_quit = false;

    switch(event->type)
    {
        case SDL_QUIT:
        {
            TR_LOG_DBG("SDL_QUIT\n");
            should_quit = true;
        } break;

        case SDL_WINDOWEVENT:
        {
            switch(event->window.event)
            {
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                {
                    SDL_Window *window = SDL_GetWindowFromID(event->window.windowID);
                    SDL_Renderer *renderer = SDL_GetRenderer(window);
                    TR_LOG_DBG("SDL_WINDOWEVENT_SIZE_CHANGED (%d, %d)\n", event->window.data1, event->window.data2);
                    sdl_resize_texture(&global_back_buffer, renderer, event->window.data1, event->window.data2);
                } break;

                case SDL_WINDOWEVENT_FOCUS_GAINED:
                {
                    TR_LOG_DBG("SDL_WINDOWEVENT_FOCUS_GAINED\n");
                } break;

                case SDL_WINDOWEVENT_EXPOSED:
                {
                    SDL_Window *window = SDL_GetWindowFromID(event->window.windowID);
                    SDL_Renderer *renderer = SDL_GetRenderer(window);
                    sdl_update_window(renderer, global_back_buffer);
                } break;
            }
        } break;
    }
    return should_quit ;
}


void
sdl_open_game_controllers()
{
    int32_t num_joysticks = SDL_NumJoysticks();
    for (int32_t controller_index = 0; controller_index < num_joysticks; ++controller_index)
    {
        if (!SDL_IsGameController(controller_index))
        {
            continue;
        }

        if (controller_index >= TR_MAX_CONTROLLERS)
        {
            break;
        }

        controller_handles[controller_index] = SDL_GameControllerOpen(controller_index);
        rumble_handles[controller_index] = SDL_HapticOpen(controller_index);

        if (rumble_handles[controller_index] && SDL_HapticRumbleInit(rumble_handles[controller_index]) != 0)
        {
            SDL_HapticClose(rumble_handles[controller_index]);
            rumble_handles[controller_index] = 0;
        }
    }
}


void
sdl_close_game_controllers()
{
    for (int32_t controller_index = 0; controller_index < TR_MAX_CONTROLLERS; ++controller_index)
    {
        if (controller_handles[controller_index])
        {
            SDL_GameControllerClose(controller_handles[controller_index]);
        }
    }
}


void
sdl_cleanup(void)
{
    TR_LOG_DBG("Cleaning up...\n");
    sdl_close_game_controllers();
    SDL_Quit();
}


int
main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) != 0)
    {
        TR_LOG_ERR("SDL_Init failed: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    sdl_open_game_controllers();

    // We register this function only _after_ we init SDL and open the game
    // controllers!
    atexit(sdl_cleanup);

    SDL_Window *window = SDL_CreateWindow(
            "Tunnel Runner",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            TR_SCREEN_WIDTH,
            TR_SCREEN_HEIGHT,
            0);

    if (window)
    {
        SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);

        if (renderer)
        {
            struct SDLWindowDimension dimension = sdl_get_window_dimension(window);
            sdl_resize_texture(&global_back_buffer, renderer, dimension.width, dimension.height);

            uint32_t texture[TR_TEX_HEIGHT][TR_TEX_WIDTH];

            for (int32_t y = 0; y < TR_TEX_HEIGHT; ++y)
            {
                for (int32_t x = 0; x < TR_TEX_WIDTH; ++x)
                {
                    // XOR texture:
                    texture[y][x] = (x * 256 / TR_TEX_WIDTH) ^ (y * 256 / TR_TEX_HEIGHT);
                    // Mosaic texture:
                    //texture[y][x] = (x * x * y * y);
                }
            }

            bool running = true;
            int32_t rotation_offset = 0;
            int32_t translation_offset = 0;
            enum Color color_choice = COLOR_WHITE;

            uint64_t lag = 0;
            uint64_t previous_ms = get_current_time_ms();

            while (running)
            {
                uint64_t current_ms = get_current_time_ms();
                uint64_t elapsed_ms = current_ms - previous_ms;
                previous_ms = current_ms;
                lag += elapsed_ms;
                TR_LOG_FRM("Lag: %d\n", lag);

                TR_LOG_FRM("%" PRIu64 ", %f\n", lag, TR_MS_PER_UPDATE);
                // TODO: I don't think we need determinism
                while (lag >= TR_MS_PER_UPDATE)
                {
                    SDL_Event event;

                    while (SDL_PollEvent(&event))
                    {
                        running = !handle_event(&event);
                    }

                    SDL_PumpEvents();

                    dimension = sdl_get_window_dimension(window);

                    const uint8_t *keystate = SDL_GetKeyboardState(0);

                    if (keystate[SDL_SCANCODE_A])
                    {
                        rotation_offset -= TR_MOVEMENT_SPEED;
                    }
                    if (keystate[SDL_SCANCODE_D])
                    {
                        rotation_offset += TR_MOVEMENT_SPEED;
                    }
                    if (keystate[SDL_SCANCODE_W])
                    {
                        translation_offset += TR_MOVEMENT_SPEED;
                    }
                    if (keystate[SDL_SCANCODE_S])
                    {
                        translation_offset -= TR_MOVEMENT_SPEED;
                    }
                    if (keystate[SDL_SCANCODE_LEFT])
                    {
                        rotation_offset --;
                    }
                    if (keystate[SDL_SCANCODE_RIGHT])
                    {
                        rotation_offset ++;
                    }
                    if (keystate[SDL_SCANCODE_UP])
                    {
                        translation_offset ++;
                    }
                    if (keystate[SDL_SCANCODE_DOWN])
                    {
                        translation_offset --;
                    }


                    for (int32_t controller_index = 0; controller_index < TR_MAX_CONTROLLERS; ++controller_index)
                    {
                        if (SDL_GameControllerGetAttached(controller_handles[controller_index]))
                        {
                            bool start = SDL_GameControllerGetButton(controller_handles[controller_index], SDL_CONTROLLER_BUTTON_START);
                            bool back = SDL_GameControllerGetButton(controller_handles[controller_index], SDL_CONTROLLER_BUTTON_BACK);
                            bool a_button = SDL_GameControllerGetButton(controller_handles[controller_index], SDL_CONTROLLER_BUTTON_A);
                            bool b_button = SDL_GameControllerGetButton(controller_handles[controller_index], SDL_CONTROLLER_BUTTON_B);
                            bool x_button = SDL_GameControllerGetButton(controller_handles[controller_index], SDL_CONTROLLER_BUTTON_X);
                            bool y_button = SDL_GameControllerGetButton(controller_handles[controller_index], SDL_CONTROLLER_BUTTON_Y);
                            bool left_shoulder = SDL_GameControllerGetButton(controller_handles[controller_index], SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
                            bool right_shoulder = SDL_GameControllerGetButton(controller_handles[controller_index], SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);

                            int16_t stick_leftx = SDL_GameControllerGetAxis(controller_handles[controller_index], SDL_CONTROLLER_AXIS_LEFTX);
                            int16_t stick_lefty = SDL_GameControllerGetAxis(controller_handles[controller_index], SDL_CONTROLLER_AXIS_LEFTY);
                            int16_t stick_rightx = SDL_GameControllerGetAxis(controller_handles[controller_index], SDL_CONTROLLER_AXIS_RIGHTX);
                            int16_t stick_righty = SDL_GameControllerGetAxis(controller_handles[controller_index], SDL_CONTROLLER_AXIS_RIGHTY);

                            if (start)
                            {
                                SDL_HapticRumblePlay(rumble_handles[controller_index], 0.5f, 2000);
                                color_choice = '\0';
                            }
                            else
                            {
                                SDL_HapticRumbleStop(rumble_handles[controller_index]);
                            }

                            if (back)
                            {
                                running = false;
                            }

                            // NOTE: Buttons select colors.
                            if (a_button)
                            {
                                color_choice = COLOR_GREEN;
                            }
                            if (b_button)
                            {
                                color_choice = COLOR_RED;
                            }
                            if (x_button)
                            {
                                color_choice = COLOR_BLUE;
                            }
                            if (y_button)
                            {
                                color_choice = COLOR_YELLOW;
                            }
                            if (left_shoulder)
                            {
                                color_choice = COLOR_MAGENTA;
                            }
                            if (right_shoulder)
                            {
                                color_choice = COLOR_CYAN;
                            }

                            rotation_offset += stick_leftx / 5000;
                            translation_offset -= stick_lefty / 5000;

                            int32_t dampened_x_max = dimension.width / 2;
                            int32_t dampened_x_min = -(dimension.width / 2);
                            int32_t dampened_y_max = dimension.height / 2;
                            int32_t dampened_y_min = -(dimension.height / 2);

                            int32_t dampened_x = (stick_rightx - TR_CONTROLLER_STICK_MIN) * (dampened_x_max - dampened_x_min) / (TR_CONTROLLER_STICK_MAX - TR_CONTROLLER_STICK_MIN) + dampened_x_min;
                            int32_t dampened_y = (stick_righty - TR_CONTROLLER_STICK_MIN) * (dampened_y_max - dampened_y_min) / (TR_CONTROLLER_STICK_MAX - TR_CONTROLLER_STICK_MIN) + dampened_y_min;

                            transform.look_shift_x = dimension.width / 2 + dampened_x;
                            transform.look_shift_y = dimension.height / 2 + dampened_y;
                            TR_LOG_FRM("dimension.width / 2: %d\t damp_x: %d\t raw_x: %d\n", dimension.width / 2, dampened_x, stick_rightx);
                            TR_LOG_FRM("dimension.height / 2: %d\t damp_y: %d\t raw_y: %d\n", dimension.height / 2, dampened_y, stick_righty);
                        }
                    }
                    TR_LOG_FRM("%d, %d\n", translation_offset, rotation_offset);

                    TR_LOG_FRM("\t%" PRIu64 ", %f\n", lag, TR_MS_PER_UPDATE);
                    //render_tunnel(global_back_buffer, texture, rotation_offset, translation_offset, color_choice);
                    lag -= TR_MS_PER_UPDATE;
                }
                render_tunnel(global_back_buffer, texture, rotation_offset, translation_offset, color_choice);
                //render_texture(global_back_buffer, texture, rotation_offset, translation_offset, color_choice);
                sdl_update_window(renderer, global_back_buffer);
                if (elapsed_ms <= TR_MS_PER_FRAME)
                {
                    SDL_Delay((TR_MS_PER_FRAME - elapsed_ms));
                }
            }
        }
        else
        {
            TR_LOG_ERR("SDL_CreateRenderer failed: %s\n", SDL_GetError());
            exit(EXIT_FAILURE);
        }
    }
    else
    {
        TR_LOG_ERR("SDL_CreateWindow failed: %s\n", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    return 0;
}
