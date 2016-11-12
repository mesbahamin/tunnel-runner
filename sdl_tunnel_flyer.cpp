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

#define BYTES_PER_PIXEL 4


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
                    sdl_resize_texture(&global_back_buffer, renderer, event->window.data1, event->window.data2);
                } break;

                case SDL_WINDOWEVENT_FOCUS_GAINED:
                {
                    printf("SDL_WINDOWEVENT_FOCUS_GAINED\n");
                } break;

                case SDL_WINDOWEVENT_EXPOSED:
                {
                    SDL_Window *window = SDL_GetWindowFromID(event->window.windowID);
                    SDL_Renderer *renderer = SDL_GetRenderer(window);
                    sdl_update_window(window, renderer, global_back_buffer);
                } break;
            }
        } break;
    }
    return(should_quit);
}


int main(void)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        // TODO(amin): log SDL_Init error
    }

    SDL_Window *window = SDL_CreateWindow(
            "Tunnel Flyer",
            SDL_WINDOWPOS_UNDEFINED,
            SDL_WINDOWPOS_UNDEFINED,
            640,
            480,
            SDL_WINDOW_RESIZABLE);

    if (window)
    {
        SDL_Renderer *renderer = SDL_CreateRenderer(window, -1, 0);

        if (renderer)
        {
            bool running = true;
            SDLWindowDimension dimension = sdl_get_window_dimension(window);
            sdl_resize_texture(&global_back_buffer, renderer, dimension.width, dimension.height);

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

    SDL_Quit();
    return(0);
}
