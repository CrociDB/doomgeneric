#include "doomkeys.h"
#include "doomgeneric.h"
#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>

#define ORIGINAL_SCALE          0.333

#define EMOJI
#define EMOJI
#define EMOJI_FILE              "emoji_all.png"
#define EMOJI_WIDTH             18
#define EMOJI_HEIGHT            18
#define EMOJI_TABLE_MAX_COL     60
#define EMOJI_COLOR_DIV         1.0
#define EMOJI_COLOR_RATE        .79

static SDL_Window* window;
static SDL_Renderer* renderer;
static SDL_Event event_handler;

#ifdef EMOJI
typedef struct _emoji_data
{
    SDL_Rect rect;
    uint8_t saturation;
}emoji_data;

typedef struct _emoji_table
{
    emoji_data red[EMOJI_TABLE_MAX_COL];
    emoji_data nred[EMOJI_TABLE_MAX_COL];
    emoji_data green[EMOJI_TABLE_MAX_COL];
    emoji_data ngreen[EMOJI_TABLE_MAX_COL];
    emoji_data blue[EMOJI_TABLE_MAX_COL];
    emoji_data nblue[EMOJI_TABLE_MAX_COL];
    emoji_data gray[EMOJI_TABLE_MAX_COL];

    uint8_t r, nr, g, ng, b, nb, gr;
}emoji_t;

static emoji_t emoji_table;

static SDL_Surface* emojis = NULL;
static SDL_Surface* screen = NULL;
#endif

#define KEYQUEUE_SIZE 16
static unsigned short s_KeyQueue[KEYQUEUE_SIZE];
static unsigned int s_KeyQueueWriteIndex = 0;
static unsigned int s_KeyQueueReadIndex = 0;

void DG_Destroy()
{
#ifdef EMOJI
    SDL_FreeSurface(emojis);
    SDL_FreeSurface(screen);
#else
    SDL_DestroyRenderer(renderer);
#endif
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

#ifdef EMOJI
Uint32 getpixel(SDL_Surface* surface, int x, int y)
{
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to retrieve */
    Uint8* p = (Uint8*)surface->pixels + y * surface->pitch + x * bpp;

    switch (bpp) {
    case 1:
        return *p;
        break;

    case 2:
        return *(Uint16*)p;
        break;

    case 3:
        if (SDL_BYTEORDER == SDL_BIG_ENDIAN)
            return p[0] << 16 | p[1] << 8 | p[2];
        else
            return p[0] | p[1] << 8 | p[2] << 16;
        break;

    case 4:
        return *(Uint32*)p;
        break;

    default:
        return 0;       /* shouldn't happen, but avoids warnings */
    }
}

void sort_color_table(emoji_data* table, int size)
{
    for (int i = 0; i < size; i++)
    for (int j = i; j < size; j++)
    {
        if (table[i].saturation > table[j].saturation)
        {
            emoji_data tmp = table[i];
            table[i] = table[j];
            table[j] = tmp;
        }
    }
}

inline int search_saturation_position(emoji_data* table, int saturation, int size)
{
    for (int i = 0; i < size; i++)
    {
        if (table[i].saturation == saturation)
        {
            return i;
        }
    }

    return -1;
}

inline emoji_data get_emoji_saturation(emoji_data* table, int saturation, int size)
{
    for (int i = size - 1; i >= 0; i--)
    {
        if (table[i].saturation < saturation)
        {
            return table[i];
        }
    }

    return table[0];
}

void create_emoji_table()
{
    emoji_table.r = 0;
    emoji_table.nr = 0;
    emoji_table.g = 0;
    emoji_table.ng = 0;
    emoji_table.b = 0;
    emoji_table.nb = 0;
    emoji_table.gr = 0;

    int w = emojis->w / EMOJI_WIDTH;
    int h = emojis->h / EMOJI_HEIGHT;

    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            emoji_data d;
            d.rect.x = x * EMOJI_WIDTH;
            d.rect.y = y * EMOJI_HEIGHT;
            d.rect.w = EMOJI_WIDTH;
            d.rect.h = EMOJI_HEIGHT;

            int total = EMOJI_WIDTH * EMOJI_HEIGHT;

            int r = 0, g = 0, b = 0;
            for (int ay = y * EMOJI_HEIGHT; ay < y * EMOJI_HEIGHT + EMOJI_HEIGHT; ay++)
                for (int ax = x * EMOJI_WIDTH; ax < x * EMOJI_WIDTH + EMOJI_WIDTH; ax++)
                {
                    uint32_t col = getpixel(emojis, ax, ay);
                    int ar = 0, ag = 0, ab = 0;
                    SDL_GetRGB(col, emojis->format, &ar, &ag, &ab);
                    r += ar;
                    g += ag;
                    b += ab;
                    //if (r + g + b == 0) total--;
                }

            if (total == 0) total = 1;

            r /= total;
            g /= total;
            b /= total;
            r *= EMOJI_COLOR_DIV;
            g *= EMOJI_COLOR_DIV;
            b *= EMOJI_COLOR_DIV;

            d.saturation = (r + g + b) / 3;

            float xr = EMOJI_COLOR_RATE;
            float ur = 1.0 - EMOJI_COLOR_RATE;

            if (r > g + g * ur && r > b + b * ur)
            {
                if (search_saturation_position(emoji_table.red, d.saturation, EMOJI_TABLE_MAX_COL) == -1)
                {
                    emoji_table.red[emoji_table.r] = d;
                    emoji_table.r = (emoji_table.r + 1) % EMOJI_TABLE_MAX_COL;
                }
            }
            else if (g > r + r * ur && g > b + b * ur)
            {
                if (search_saturation_position(emoji_table.green, d.saturation, EMOJI_TABLE_MAX_COL) == -1)
                {
                    emoji_table.green[emoji_table.g] = d;
                    emoji_table.g = (emoji_table.g + 1) % EMOJI_TABLE_MAX_COL;
                }
            }
            else if (b > g + g * ur && b > r + r * ur)
            {
                if (search_saturation_position(emoji_table.blue, d.saturation, EMOJI_TABLE_MAX_COL) == -1)
                {
                    emoji_table.blue[emoji_table.b] = d;
                    emoji_table.b = (emoji_table.b + 1) % EMOJI_TABLE_MAX_COL;
                }
            }
            else if (r < g * xr && r < b * xr)
            {
                if (search_saturation_position(emoji_table.nred, d.saturation, EMOJI_TABLE_MAX_COL) == -1)
                {
                    emoji_table.nred[emoji_table.nr] = d;
                    emoji_table.nr = (emoji_table.nr + 1) % EMOJI_TABLE_MAX_COL;
                }
            }
            else if (g < r * xr && g < b * xr)
            {
                if (search_saturation_position(emoji_table.ngreen, d.saturation, EMOJI_TABLE_MAX_COL) == -1)
                {
                    emoji_table.ngreen[emoji_table.ng] = d;
                    emoji_table.ng = (emoji_table.ng + 1) % EMOJI_TABLE_MAX_COL;
                }
            }
            else if (b < g * xr && b < r * xr)
            {
                if (search_saturation_position(emoji_table.nblue, d.saturation, EMOJI_TABLE_MAX_COL) == -1)
                {
                    emoji_table.nblue[emoji_table.nb] = d;
                    emoji_table.nb = (emoji_table.nb + 1) % EMOJI_TABLE_MAX_COL;
                }
            }
            else
            {
                if (search_saturation_position(emoji_table.gray, d.saturation, EMOJI_TABLE_MAX_COL) == -1)
                {
                    emoji_table.gray[emoji_table.gr] = d;
                    emoji_table.gr = (emoji_table.gr + 1) % EMOJI_TABLE_MAX_COL;
                }
            }
        }

        sort_color_table(emoji_table.red, EMOJI_TABLE_MAX_COL);
        sort_color_table(emoji_table.nred, EMOJI_TABLE_MAX_COL);
        sort_color_table(emoji_table.green, EMOJI_TABLE_MAX_COL);
        sort_color_table(emoji_table.ngreen, EMOJI_TABLE_MAX_COL);
        sort_color_table(emoji_table.blue, EMOJI_TABLE_MAX_COL);
        sort_color_table(emoji_table.nblue, EMOJI_TABLE_MAX_COL);
        sort_color_table(emoji_table.gray, EMOJI_TABLE_MAX_COL);
    }
}

emoji_data get_emoji(uint8_t r, uint8_t g, uint8_t b)
{
    float xr = EMOJI_COLOR_RATE;
    float ur = 1.0 - EMOJI_COLOR_RATE;
    int saturation = ((r + g + b) / 3) * EMOJI_COLOR_DIV;
    if (r > g + g * ur && r > b + b * ur)
    {
        return get_emoji_saturation(emoji_table.red, saturation, EMOJI_TABLE_MAX_COL);
    }
    else if (g > r + r * ur && g > b + b * ur)
    {
        return get_emoji_saturation(emoji_table.green, saturation, EMOJI_TABLE_MAX_COL);
    } 
    else if (b > g + g * ur && b > r + r * ur)
    {
        return get_emoji_saturation(emoji_table.blue, saturation, EMOJI_TABLE_MAX_COL);
    }
    else if (r < g * xr && r < b * xr)
    {
        return get_emoji_saturation(emoji_table.nred, saturation, EMOJI_TABLE_MAX_COL);
    }
    else if (g < r * xr && g < b * xr)
    {
        return get_emoji_saturation(emoji_table.ngreen, saturation, EMOJI_TABLE_MAX_COL);
    }
    else if (b < g * xr && b < r * xr)
    {
        return get_emoji_saturation(emoji_table.nblue, saturation, EMOJI_TABLE_MAX_COL);
    }

    return get_emoji_saturation(emoji_table.gray, saturation, EMOJI_TABLE_MAX_COL);
}
#endif

void DG_Init()
{
    int a = DOOMGENERIC_RESX * ORIGINAL_SCALE * EMOJI_WIDTH - 10;
    int b = DOOMGENERIC_RESY * ORIGINAL_SCALE * EMOJI_HEIGHT - 10;

    window = SDL_CreateWindow(
        "Doomoji",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        a,
        b,
        SDL_WINDOW_SHOWN);


    if (window == NULL)
    {
        printf("Couldn't create window. SDL Error: %s", SDL_GetError());
        exit(-1);
    }

#ifdef EMOJI
    IMG_Init(IMG_INIT_PNG);

    screen = SDL_GetWindowSurface(window);
    emojis = IMG_Load(EMOJI_FILE);

    create_emoji_table();
#else
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
#endif

    memset(s_KeyQueue, 0, KEYQUEUE_SIZE * sizeof(unsigned short));
}

void DG_DrawFrame()
{
    _event_loop();

    int h = DOOMGENERIC_RESY * ORIGINAL_SCALE;
    int w = DOOMGENERIC_RESX * ORIGINAL_SCALE;

    int hm = DOOMGENERIC_RESY / h;
    int wm = DOOMGENERIC_RESX / w;
    int total = hm * wm;

    int x, y;
    for (y = 0; y < h; y++)
    {
        for (x = 0; x < w; x++)
        {
            int r = 0, g = 0, b = 0;
            for (int ay = 0; ay < hm; ay++)
            for (int ax = 0; ax < wm; ax++)
            {
                int col = DG_ScreenBuffer[((y * hm) + ay) * DOOMGENERIC_RESX + ((x * wm) + ax)];
                r += (uint8_t)(col >> 16);
                g += (uint8_t)(col >> 8);
                b += (uint8_t)col;
            }

            r /= total;
            g /= total;
            b /= total;
                

#ifdef EMOJI
            SDL_Rect rect;
            rect.x = x* EMOJI_WIDTH;
            rect.y = y* EMOJI_HEIGHT;

            SDL_Rect ra;
            ra.x = 0 * EMOJI_WIDTH;
            ra.y = 0 * EMOJI_HEIGHT;
            ra.w = EMOJI_WIDTH;
            ra.h = EMOJI_HEIGHT;

            emoji_data emoji = get_emoji(r, g, b);
            SDL_BlitSurface(emojis, &emoji.rect, screen, &rect);

#else
            SDL_SetRenderDrawColor(renderer, r, g, b, 0xff);
            SDL_RenderDrawPoint(renderer, x, y);
#endif
        }
    }

#ifdef EMOJI
    SDL_UpdateWindowSurface(window);
#else
    SDL_RenderPresent(renderer);
#endif
}
