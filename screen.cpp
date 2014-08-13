#ifdef MIPS
    #define L_BUTTON      "tab"
    #define R_BUTTON      "backspace"

    #define X_BUTTON      "left shift"
    #define Y_BUTTON      "space"
    #define A_BUTTON      "left ctrl"
    #define B_BUTTON      "left alt"

    #define SELECT_BUTTON ""
    #define START_BUTTON  ""

    #define UP_BUTTON     "up"
    #define DOWN_BUTTON   "down"
    #define LEFT_BUTTON   "left"
    #define RIGHT_BUTTON  "right"

    #define PAUSE_BUTTON  "pause"

    /* select + start button pressed togheter. */
    #define RETURN_BUTTON "return"
#else
    #define L_BUTTON      SDLK_l
    #define R_BUTTON      SDLK_r

    #define X_BUTTON      SDLK_x
    #define Y_BUTTON      SDLK_y
    #define A_BUTTON      SDLK_a
    #define B_BUTTON      SDLK_b

    #define SELECT_BUTTON SDLK_ESCAPE
    #define START_BUTTON  ""

    #define UP_BUTTON     "up"
    #define DOWN_BUTTON   "down"
    #define LEFT_BUTTON   "left"
    #define RIGHT_BUTTON  "right"

    #define PAUSE_BUTTON  "pause"

    /* select + start button pressed togheter. */
    #define RETURN_BUTTON "return"
#endif


#include <SDL.h>
#include <SDL_image.h>
#include <string>
#include <iostream>

using namespace std;

#define WIDTH  320
#define HEIGHT 240
#define DEPTH  8

SDL_Surface *screen,
            *rec_btn,
            *stop_btn,
            *play_btn,
            *pause_btn;

bool is_recording = false,
     is_playing = false;


void apply_surface(SDL_Surface *image, int x, int y);
void apply_surface(SDL_Surface *image, int x, int y, int w, int h);
void draw_rec_stop();
void draw_play_pause();
SDL_Surface *load_png(string path);
void load_resources();
void main_loop();

void apply_surface(SDL_Surface *image, int x, int y)
{
    SDL_Rect tmp;

    tmp.x = x;
    tmp.y = y;

    // Apply the image to the display
    if (SDL_BlitSurface(image, NULL, screen, &tmp) != 0)
    {
        printf("SDL_BlitSurface() Failed: ", SDL_GetError());
    }

    SDL_Flip(screen);
}

void apply_surface(SDL_Surface *image, int x, int y, int w, int h)
{
    Uint32 color = SDL_MapRGB(screen->format, 0, 0, 0);

    SDL_Rect tmp;
    tmp.x = x;
    tmp.y = y;
    tmp.w = w;
    tmp.h = h;

    SDL_FillRect(screen, &tmp, color);

    apply_surface(image, tmp.x, tmp.y);
}

void draw_rec_stop()
{
    SDL_Surface *source = is_recording ? stop_btn : rec_btn;
    apply_surface(source, 126, 120, 32, 32);
}

void draw_play_pause()
{
    SDL_Surface *source = is_playing ? pause_btn : play_btn;
    apply_surface(source, 162, 120, 32, 32);
}

SDL_Surface *load_png(string path)
{
    SDL_Surface *image = IMG_Load(path.c_str());

    if(!image)
    {
        printf("IMG_Load: %s\n", IMG_GetError());
    }

    return image;
}

void load_resources()
{
    rec_btn = load_png("resources/32/player_record.png");
    stop_btn = load_png("resources/32/player_stop.png");
    play_btn = load_png("resources/32/player_play.png");
    pause_btn = load_png("resources/32/player_pause.png");
}

void main_loop()
{
    SDL_Event event;
    bool quit = false;

    while(!quit && SDL_WaitEvent(&event))
    {
        switch(event.type)
        {
            case SDL_KEYDOWN:
            //case SDL_KEYUP:
            {
                SDLKey key = event.key.keysym.sym;
                switch (key)
                {
                    case SELECT_BUTTON:
                        quit = true;
                    break;
                    case A_BUTTON:
                        is_recording = !is_recording;
                        draw_rec_stop();
                    break;
                    case B_BUTTON:
                        is_playing = !is_playing;
                        draw_play_pause();
                    break;
                    default:
                    break;
                }
            }
            break;
            case SDL_QUIT: /* window close */
                quit = true;
            break;
            default:
            break;
        }
    }
}


int main()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        fprintf(stderr, "Cannot init SDL. Aborting.\n");
        return 1;
    }

    if (!(screen = SDL_SetVideoMode(WIDTH, HEIGHT, DEPTH, SDL_HWSURFACE | SDL_DOUBLEBUF)))
    {
        fprintf(stderr, "Cannot SetVideoMode. Aborting.\n");
        SDL_Quit();
        return 1;
    }

    SDL_ShowCursor(SDL_DISABLE);

    int imgFlags = IMG_INIT_PNG; 
    if(!(IMG_Init(imgFlags) & imgFlags))
    {
        printf("SDL_image could not initialize! SDL_image Error: %s\n", IMG_GetError());
        return 1;
        SDL_Quit();
    }

    load_resources();

    draw_rec_stop();
    draw_play_pause();

/*
SDL_Rect r = {0, 0, 10, 10};
SDL_FillRect(screen, &r, SDL_MapRGB(screen->format, 255, 0, 0));
SDL_Flip(screen);
*/
    main_loop();
    IMG_Quit();
    SDL_Quit();

    return 0;
}