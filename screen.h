#ifndef SCREEN_H
#define SCREEN_H 1

#ifdef __MIPSEL__
    #define L_BUTTON      SDLK_TAB
    #define R_BUTTON      SDLK_BACKSPACE

    #define X_BUTTON      SDLK_LSHIFT
    #define Y_BUTTON      SDLK_SPACE
    #define A_BUTTON      SDLK_LCTRL
    #define B_BUTTON      SDLK_LALT

    std::string BASE_PATH = std::string(std::getenv("HOME")) + "/.voice/";
#else
    #define L_BUTTON      SDLK_l
    #define R_BUTTON      SDLK_r

    #define X_BUTTON      SDLK_x
    #define Y_BUTTON      SDLK_y
    #define A_BUTTON      SDLK_a
    #define B_BUTTON      SDLK_b

    std::string BASE_PATH = "./";
#endif

#define SELECT_BUTTON SDLK_ESCAPE
#define START_BUTTON  SDLK_RETURN

#define UP_BUTTON     SDLK_UP
#define DOWN_BUTTON   SDLK_DOWN
#define LEFT_BUTTON   SDLK_LEFT
#define RIGHT_BUTTON  SDLK_RIGHT

#define PAUSE_BUTTON  SDLK_PAUSE

#include <SDL.h>
#include <SDL/SDL_ttf.h>
#include <SDL_image.h>
#include <string>

#define WIDTH  320
#define HEIGHT 240
#define DEPTH  8

void apply_surface(SDL_Surface *image, int x, int y);
void apply_surface(SDL_Surface *image, int x, int y, int w, int h);
void draw_rec_stop();
void draw_play_pause();
SDL_Surface *load_png(std::string path);
void load_resources();
void main_loop();
void draw_buttons();
void draw_timer(signed int hours, signed int minutes, signed int seconds);
void draw_volume();
void draw_vu(signed int perc, int direction);

#endif