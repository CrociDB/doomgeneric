#include "doomkeys.h"
#include "doomgeneric.h"
#include <stdio.h>
#include <SDL.h>

SDL_Window* window;
SDL_Renderer* renderer;
SDL_Event event_handler;
uint8_t scale;

#define KEYQUEUE_SIZE 16
static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

void DG_Destroy()
{
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    free(DG_ScreenBuffer);
    exit(0);
}

static unsigned char convertToDoomKey(uint32_t key)
{
    switch (key)
    {
    case SDLK_RETURN:
        key = KEY_ENTER;
        break;
    case SDLK_ESCAPE:
        key = KEY_ESCAPE;
        break;
    case SDLK_a:
    case SDLK_LEFT:
        key = KEY_LEFTARROW;
        break;
    case SDLK_d:
    case SDLK_RIGHT:
        key = KEY_RIGHTARROW;
        break;
    case SDLK_w:
    case SDLK_UP:
        key = KEY_UPARROW;
        break;
    case SDLK_s:
    case SDLK_DOWN:
        key = KEY_DOWNARROW;
        break;
    case SDLK_LCTRL:
        key = KEY_FIRE;
        break;
    case SDLK_SPACE:
        key = KEY_USE;
        break;
    case SDLK_LSHIFT:
        key = KEY_RSHIFT;
        break;
    default:
        key = 0;
        break;
    }

    return key;
}

static void addKeyToQueue(int pressed, uint32_t keyCode)
{
    unsigned char key = convertToDoomKey(keyCode);

    unsigned short keyData = (pressed << 8) | key;

    s_KeyQueue[s_KeyQueueWriteIndex] = keyData;
    s_KeyQueueWriteIndex++;
    s_KeyQueueWriteIndex %= KEYQUEUE_SIZE;
}

void _event_loop()
{
    while (SDL_PollEvent(&event_handler) != 0)
    {
        if (event_handler.type == SDL_WINDOWEVENT)
        {
            if (event_handler.window.event == SDL_WINDOWEVENT_CLOSE)
            {
                DG_Destroy();
            }
        }
        if (event_handler.type == SDL_QUIT)
        {
            DG_Destroy();
        }
        else if (event_handler.type == SDL_KEYDOWN)
        {
            addKeyToQueue(1, event_handler.key.keysym.sym);
        }
        else if (event_handler.type == SDL_KEYUP)
        {
            addKeyToQueue(0, event_handler.key.keysym.sym);
        }
    }
}

int DG_GetKey(int* pressed, unsigned char* doomKey)
{
    if (s_KeyQueueReadIndex == s_KeyQueueWriteIndex)
    {
        //key queue is empty

        return 0;
    }
    else
    {
        unsigned short keyData = s_KeyQueue[s_KeyQueueReadIndex];
        s_KeyQueueReadIndex++;
        s_KeyQueueReadIndex %= KEYQUEUE_SIZE;

        *pressed = keyData >> 8;
        *doomKey = keyData & 0xFF;

        return 1;
    }
}

void DG_SleepMs(uint32_t ms)
{
    SDL_Delay(ms);
}

uint32_t DG_GetTicksMs()
{
    return SDL_GetTicks();
}

void DG_SetWindowTitle(const char* title)
{
    SDL_SetWindowTitle(window, title);
}

void DG_Init()
{
    scale = 1;
    window = SDL_CreateWindow(
        "Doomoji",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        DOOMGENERIC_RESX * scale,
        DOOMGENERIC_RESY * scale,
        SDL_WINDOW_SHOWN);

    if (window == NULL)
    {
        printf("Couldn't create window. SDL Error: %s", SDL_GetError());
        exit(-1);
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);

    memset(s_KeyQueue, 0, KEYQUEUE_SIZE * sizeof(unsigned short));
}

void DG_DrawFrame()
{
    _event_loop();

    int x, y;
    for (y = 0; y < DOOMGENERIC_RESY; y++)
    {
        for (x = 0; x < DOOMGENERIC_RESX; x++)
        {
            int col = DG_ScreenBuffer[y * DOOMGENERIC_RESX + x];

            SDL_SetRenderDrawColor(renderer, col >> 16, col >> 8, col, 0xff);
            SDL_RenderDrawPoint(renderer, x, y);
        }
    }

    SDL_RenderPresent(renderer);
}
