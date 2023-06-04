#include "config.h"
#include <stdio.h>
#include <SDL2/SDL.h>
#include "control.h"
#include "utils.h"
#include "framebuffer.h"

static SDL_Window* window = NULL;
static SDL_Keycode isKeyDown = SDLK_UNKNOWN;


#define FPS_INTERVAL 1.0 //seconds. 

Uint32 fps_lasttime = SDL_GetTicks(); //the last recorded time. 
Uint32 fps_current; //the current FPS. 
Uint32 fps_frames = 0; //frames passed since the last recorded fps. 

void guiDrawBitmap(uint16_t* bmp, int32_t x, int32_t y, int32_t w, int32_t h)
{
    SDL_Surface* surface = SDL_GetWindowSurface(window);
    /*
    if (SDL_MUSTLOCK(surface))
    {
        if (SDL_LockSurface(surface) != 0)
        {
            printf("SDL_LockSurface err\n");
        }
    }
    */
    for (int32_t j = 0; j < h; j++)
    {
        for (int32_t i = 0; i < w; i++)
        {
            int32_t xx = x + i;
            int32_t yy = y + j;
            if (xx < 0 || yy < 0 || xx >= SCREEN_WIDTH || yy >= SCREEN_HEIGHT)
            {
                continue;
            }
            uint16_t color = *(bmp + (xx + yy * SCREEN_WIDTH));
            Uint32* p = (Uint32*)(((Uint8*)surface->pixels) + surface->pitch * yy) + xx;
            *p = SDL_MapRGB(surface->format, PIXEL565R(color), PIXEL565G(color), PIXEL565B(color));
        }
    }
    /*
    if (SDL_MUSTLOCK(surface))
    {
        SDL_UnlockSurface(surface);
    }
    */
    if (SDL_UpdateWindowSurface(window) != 0)
    {
        printf("SDL_UpdateWindowSurface err\n");
    }

    fps_frames++;
    if (fps_lasttime < SDL_GetTicks() - FPS_INTERVAL * 1000)
    {
        fps_lasttime = SDL_GetTicks();
        fps_current = fps_frames;
        fps_frames = 0;
        printf("fps:%d\n", fps_current);
    }
}

void updateFb(void)
{
    uint16_t* fb = (uint16_t*)getFramebuffPtr();
    guiDrawBitmap(fb, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
}

void loop()
{
    SDL_Event ev;
    bool isLoop = true;

    while (isLoop)
    {
        while (SDL_WaitEvent(&ev))
        {
            if (ev.type == SDL_QUIT)
            {
                isLoop = false;
                break;
            }
            switch (ev.type)
            {
            case SDL_KEYDOWN:
                if (isKeyDown == SDLK_UNKNOWN)
                {
                    isKeyDown = ev.key.keysym.sym;
                    updateKeyStatus(1, ev.key.keysym.sym);
                }
                break;
            case SDL_KEYUP:
                if (isKeyDown == ev.key.keysym.sym)
                {
                    isKeyDown = SDLK_UNKNOWN;
                    updateKeyStatus(0, ev.key.keysym.sym);
                }
                break;
            case SDL_MOUSEMOTION:
                break;
            case SDL_MOUSEBUTTONDOWN:
                break;
            case SDL_MOUSEBUTTONUP:
                break;
            default:
                break;
            }
        }
    }
}

int main(int argc, char* argv[])
{
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    window = SDL_CreateWindow("dingoo-emu", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_OPENGL);
    if (window == NULL) {
        printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        return -1;
    }

    startDingoo();

    loop();

	return 0;
}