#include <cmath>
#include <SDL.h>
#include <stdio.h>
#include <stdint.h>
#include <sys/mman.h>

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif

#define internal static
#define local_persist static
#define global_variable static

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

// TODO(amin): Make screen size adjustable
#define SCREEN_WIDTH 640
#define SCREEN_HEIGHT 480
#define BYTES_PER_PIXEL 4
#define MAX_CONTROLLERS 4
#define MOVEMENT_SPEED 5
#define CONTROLLER_STICK_MAX 32770
#define CONTROLLER_STICK_MIN -32770


struct SDLOffscreenBuffer
{
    // NOTE(amin): pixels are always 32-bits wide. Memory order: BB GG RR XX.
    SDL_Texture *texture;
    void *memory;
    int width;
    int height;
    int pitch;
};


struct SDLWindowDimension
{
    int width;
    int height;
};


global_variable SDLOffscreenBuffer global_back_buffer;
global_variable SDL_GameController *controller_handles[MAX_CONTROLLERS];
global_variable SDL_Haptic *rumble_handles[MAX_CONTROLLERS];


internal void
render_mosaic(SDLOffscreenBuffer buffer, int x_offset, int y_offset, char color_choice)
{
    uint8 *row = (uint8 *)buffer.memory;

    for (int y = 0; y < buffer.height; ++y)
    {
        uint32 *pixel = (uint32 *)row;

        for (int x = 0; x < buffer.width; ++x)
        {
            uint8 x_factor = (x + x_offset);
            uint8 y_factor = (y + y_offset);
            uint8 color_value (x_factor * x_factor * y_factor * y_factor);
            uint32 red = color_value << 16;
            uint32 green = color_value << 8;
            uint32 blue = color_value;

            switch(color_choice)
            {
                case 'g':
                {
                    *pixel++ = green;
                } break;
                case 'r':
                {
                    *pixel++ = red;
                } break;
                case 'b':
                {
                    *pixel++ = blue;
                } break;
                case 'y':
                {
                    *pixel++ = red | green;
                } break;
                case 'm':
                {
                    *pixel++ = red | blue;
                } break;
                case 'c':
                {
                    *pixel++ = blue | green;
                } break;
                default:
                {
                    *pixel++ = red | green | blue;
                } break;
            }
        }

        row += buffer.pitch;
    }
}


SDLWindowDimension
sdl_get_window_dimension(SDL_Window *window)
{
    SDLWindowDimension result;
    SDL_GetWindowSize(window, &result.width, &result.height);
    return(result);
}


internal void
sdl_resize_texture(SDLOffscreenBuffer *buffer, SDL_Renderer *renderer, int width, int height)
{
    if (buffer->memory)
    {
        munmap(buffer->memory, buffer->width * buffer->height * BYTES_PER_PIXEL);
    }

    if (buffer->texture)
    {
        SDL_DestroyTexture(buffer->texture);
    }

    buffer->texture = SDL_CreateTexture(
            renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            width, height);

    buffer->width = width;
    buffer->height = height;
    buffer->pitch = width * BYTES_PER_PIXEL;

    buffer->memory = mmap(
            0,
            width * height * BYTES_PER_PIXEL,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1, 0);
}


internal void
sdl_update_window(SDL_Window *window, SDL_Renderer *renderer, SDLOffscreenBuffer buffer)
{
    if (SDL_UpdateTexture(buffer.texture, 0, buffer.memory, buffer.pitch))
    {
        // TODO(amin): Handle this error
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
            printf("SDL_QUIT\n");
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
                    printf("SDL_WINDOWEVENT_SIZE_CHANGED (%d, %d)\n", event->window.data1, event->window.data2);
                    // TODO(amin): Why does this segfault?
                    //sdl_resize_texture(&global_back_buffer, renderer, event->window.data1, event->window.data2);
                } break;

                case SDL_WINDOWEVENT_FOCUS_GAINED:
                {
                    printf("SDL_WINDOWEVENT_FOCUS_GAINED\n");
                } break;

                case SDL_WINDOWEVENT_EXPOSED:
                {
                    SDL_Window *window = SDL_GetWindowFromID(event->window.windowID);
                    SDL_Renderer *renderer = SDL_GetRenderer(window);
                    //sdl_update_window(window, renderer, global_back_buffer);
                } break;
            }
        } break;
    }
    return(should_quit);
}


internal void
sdl_open_game_controllers()
{
    int num_joysticks = SDL_NumJoysticks();
    for (int controller_index = 0; controller_index < num_joysticks; ++controller_index)
    {
        if (!SDL_IsGameController(controller_index))
        {
            continue;
        }

        if (controller_index >= MAX_CONTROLLERS)
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


internal void
sdl_close_game_controllers()
{
    for (int controller_index = 0; controller_index < MAX_CONTROLLERS; ++controller_index)
    {
        if (controller_handles[controller_index])
        {
            SDL_GameControllerClose(controller_handles[controller_index]);
        }
    }
}


int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC) != 0)
    {
        // TODO(amin): log SDL_Init error
    }

    sdl_open_game_controllers();

    SDL_Window *window = SDL_CreateWindow(
            "Tunnel Flyer",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            SCREEN_WIDTH,
            SCREEN_HEIGHT,
            0);

    if (window)
    {
        SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);

        if (renderer)
        {
            bool running = true;
            SDLWindowDimension dimension = sdl_get_window_dimension(window);
            sdl_resize_texture(&global_back_buffer, renderer, dimension.width, dimension.height);

            int tex_width = 256;
            int tex_height = 256;

            uint32 texture[tex_width][tex_height];
            global_variable int distance_table[2 * SCREEN_HEIGHT][2 * SCREEN_WIDTH];
            global_variable int angle_table[2 * SCREEN_HEIGHT][2 * SCREEN_WIDTH];

            // Make XOR texture
            for (int y = 0; y < tex_height; ++y)
            {
                for (int x = 0; x < tex_width; ++x)
                {
                    texture[y][x] = (x * 256 / tex_width) ^ (y * 256 / tex_height);
                }
            }

            // Make distance and angle transformation tables
            for (int y = 0; y < 2 * SCREEN_HEIGHT; ++y)
            {
                for (int x = 0; x < 2 * SCREEN_WIDTH; ++x)
                {
                    float ratio = 32.0;
                    int distance = int(ratio * tex_height / sqrt(
                            float((x - SCREEN_WIDTH) * (x - SCREEN_WIDTH) + (y - SCREEN_HEIGHT) * (y - SCREEN_HEIGHT))
                        )) % tex_height;
                    int angle = (unsigned int)(0.5 * tex_width * atan2(float(y - SCREEN_HEIGHT), float(x - SCREEN_WIDTH)) / 3.1416);
                    distance_table[y][x] = distance;
                    angle_table[y][x] = angle;
                }
            }

            int rotation_offset = 0;
            int translation_offset = 0;
            int look_shift_x = SCREEN_WIDTH / 2;
            int look_shift_y = SCREEN_HEIGHT / 2;
            char color_choice = '\0';

            while (running)
            {
                SDL_Event event;

                while (SDL_PollEvent(&event))
                {
                    if (handle_event(&event))
                    {
                        running = false;
                    }
                }

                SDL_PumpEvents();
                const uint8 *keystate = SDL_GetKeyboardState(0);

                if (keystate[SDL_SCANCODE_A])
                {
                    rotation_offset -= MOVEMENT_SPEED;
                }
                if (keystate[SDL_SCANCODE_D])
                {
                    rotation_offset += MOVEMENT_SPEED;
                }
                if (keystate[SDL_SCANCODE_W])
                {
                    translation_offset += MOVEMENT_SPEED;
                }
                if (keystate[SDL_SCANCODE_S])
                {
                    translation_offset -= MOVEMENT_SPEED;
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


                for (int controller_index = 0; controller_index < MAX_CONTROLLERS; ++controller_index)
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

                        int16 stick_leftx = SDL_GameControllerGetAxis(controller_handles[controller_index], SDL_CONTROLLER_AXIS_LEFTX);
                        int16 stick_lefty = SDL_GameControllerGetAxis(controller_handles[controller_index], SDL_CONTROLLER_AXIS_LEFTY);
                        int16 stick_rightx = SDL_GameControllerGetAxis(controller_handles[controller_index], SDL_CONTROLLER_AXIS_RIGHTX);
                        int16 stick_righty = SDL_GameControllerGetAxis(controller_handles[controller_index], SDL_CONTROLLER_AXIS_RIGHTY);

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

                        // NOTE(amin): Buttons select colors.
                        if (a_button)
                        {
                            color_choice = 'g';
                        }
                        if (b_button)
                        {
                            color_choice = 'r';
                        }
                        if (x_button)
                        {
                            color_choice = 'b';
                        }
                        if (y_button)
                        {
                            color_choice = 'y';
                        }
                        if (left_shoulder)
                        {
                            color_choice = 'm';
                        }
                        if (right_shoulder)
                        {
                            color_choice = 'c';
                        }

                        rotation_offset += stick_leftx / 5000;
                        translation_offset -= stick_lefty / 5000;

                        int dampened_x_max = SCREEN_WIDTH / 2;
                        int dampened_x_min = -(SCREEN_WIDTH / 2);
                        int dampened_y_max = SCREEN_HEIGHT / 2;
                        int dampened_y_min = -(SCREEN_HEIGHT / 2);

                        int dampened_x = (stick_rightx - CONTROLLER_STICK_MIN) * (dampened_x_max - dampened_x_min) / (CONTROLLER_STICK_MAX - CONTROLLER_STICK_MIN) + dampened_x_min;
                        int dampened_y = (stick_righty - CONTROLLER_STICK_MIN) * (dampened_y_max - dampened_y_min) / (CONTROLLER_STICK_MAX - CONTROLLER_STICK_MIN) + dampened_y_min;

                        look_shift_x = SCREEN_WIDTH / 2 + dampened_x;
                        look_shift_y = SCREEN_HEIGHT / 2 + dampened_y;
                        //printf("screen_width / 2: %d\t damp_x: %d\t raw_x: %d\n", screen_width / 2, dampened_x, stick_rightx);
                        //printf("screen_height / 2: %d\t damp_y: %d\t raw_y: %d\n", screen_height / 2, dampened_y, stick_righty);
                    }
                }

                // Transform texture and draw to buffer
                uint8 *row = (uint8 *)global_back_buffer.memory;
                for (int y = 0; y < global_back_buffer.height; ++y)
                {
                    uint32 *pixel = (uint32 *)row;
                    for (int x = 0; x < global_back_buffer.width; ++x)
                    {
                        int color = texture[(unsigned int)(distance_table[y + look_shift_y][x + look_shift_x] + translation_offset) % tex_width]
                                           [(unsigned int)(angle_table[y + look_shift_y][x + look_shift_x] + rotation_offset) % tex_height];

                        uint32 red = color << 16;
                        uint32 green = color << 8;
                        uint32 blue = color;

                        // TODO(amin): Make a color choice enum
                        switch(color_choice)
                        {
                            case 'g':
                            {
                                *pixel++ = green;
                            } break;
                            case 'r':
                            {
                                *pixel++ = red;
                            } break;
                            case 'b':
                            {
                                *pixel++ = blue;
                            } break;
                            case 'y':
                            {
                                *pixel++ = red | green;
                            } break;
                            case 'm':
                            {
                                *pixel++ = red | blue;
                            } break;
                            case 'c':
                            {
                                *pixel++ = blue | green;
                            } break;
                            default:
                            {
                                *pixel++ = red | green | blue;
                            } break;
                        }
                    }
                    row += global_back_buffer.pitch;
                }
                //render_mosaic(global_back_buffer, rotation_offset, translation_offset, color_choice);
                sdl_update_window(window, renderer, global_back_buffer);
            }
        }
        else
        {
            // TODO(amin): log SDL_Renderer error
        }
    }
    else
    {
        // TODO(amin): log SDL_Window error
    }

    sdl_close_game_controllers();
    SDL_Quit();
    return(0);
}
