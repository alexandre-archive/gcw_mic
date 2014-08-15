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

#ifndef LOG_H
    #define LOG_H
    #include "log.h"
#endif

#include "mic.cpp"
#include "timefmt.h"

#define WIDTH  320
#define HEIGHT 240
#define DEPTH  8

SDL_Surface *screen,
            *rec_btn,
            *rec_btn_ds,
            *stop_btn,
            *play_btn,
            *play_btn_ds,
            *pause_btn;

bool is_recording = false,
     is_playing = false;

Mic *pmic;

std::string current_file;

void apply_surface(SDL_Surface *image, int x, int y);
void apply_surface(SDL_Surface *image, int x, int y, int w, int h);
void draw_rec_stop();
void draw_play_pause();
SDL_Surface *load_png(std::string path);
void load_resources();
void main_loop();
void redraw_buttons();

void apply_surface(SDL_Surface *image, int x, int y)
{
    SDL_Rect tmp;

    tmp.x = x;
    tmp.y = y;

    // Apply the image to the display
    if (SDL_BlitSurface(image, NULL, screen, &tmp) != 0)
    {
        log(ERROR, "SDL_BlitSurface() Failed: %s.", SDL_GetError());
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
    SDL_Surface *source = is_playing ? rec_btn_ds : (is_recording ? stop_btn : rec_btn);
    apply_surface(source, 126, 120, 32, 32);
}

void draw_play_pause()
{
    SDL_Surface *source = is_recording ? play_btn_ds : (is_playing ? pause_btn : play_btn);
    apply_surface(source, 162, 120, 32, 32);
}

SDL_Surface *load_png(std::string path)
{
    SDL_Surface *image = IMG_Load(path.c_str());

    if(!image)
    {
        log(ERROR, "IMG_Load: %s.", IMG_GetError());
    }

    return image;
}

void load_resources()
{
    rec_btn     = load_png("resources/32/player_record.png");
    rec_btn_ds  = load_png("resources/32/player_record_disabled.png");
    stop_btn    = load_png("resources/32/player_stop.png");
    play_btn    = load_png("resources/32/player_play.png");
    play_btn_ds = load_png("resources/32/player_play_disabled.png");
    pause_btn   = load_png("resources/32/player_pause.png");
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
                        if (is_playing) continue;

                        if (is_recording)
                        {
                            pmic->stop();
                        }
                        else
                        {
                            current_file = "rec_" + current_time() + ".wav";
                            std::cout << "file: " << current_file << std::endl;
                            pmic->record(current_file);
                        }

                        is_recording = !is_recording;
                        redraw_buttons();
                    break;
                    case B_BUTTON:
                        if (is_recording) continue;

                        if (is_playing)
                        {
                            pmic->stop();
                        }
                        else
                        {
                            std::cout << current_file << std::endl;
                            pmic->play(current_file);
                        }

                        is_playing = !is_playing;
                        redraw_buttons();
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

void redraw_buttons()
{
    draw_rec_stop();
    draw_play_pause();
}

int main()
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        log(ERROR, "Cannot init SDL. Aborting.");
        return 1;
    }

    if (!(screen = SDL_SetVideoMode(WIDTH, HEIGHT, DEPTH, SDL_HWSURFACE | SDL_DOUBLEBUF)))
    {
        log(ERROR, "Cannot SetVideoMode. Aborting.");
        SDL_Quit();
        return 1;
    }

    SDL_ShowCursor(SDL_DISABLE);

    int img_flags = IMG_INIT_PNG;

    if(!(IMG_Init(img_flags) & img_flags))
    {
        log(ERROR, "SDL_image could not initialize! SDL_image Error: %s.", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    load_resources();
    redraw_buttons();

    pmic = new Mic();

    main_loop();

    delete pmic;

    IMG_Quit();
    SDL_Quit();

    return 0;
}