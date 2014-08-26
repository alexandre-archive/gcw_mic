#ifdef MIPSEL
    #define L_BUTTON      SDLK_TAB
    #define R_BUTTON      SDLK_BACKSPACE

    #define X_BUTTON      SDLK_LSHIFT
    #define Y_BUTTON      SDLK_SPACE
    #define A_BUTTON      SDLK_LCTRL
    #define B_BUTTON      SDLK_LALT
#else
    #define L_BUTTON      SDLK_l
    #define R_BUTTON      SDLK_r

    #define X_BUTTON      SDLK_x
    #define Y_BUTTON      SDLK_y
    #define A_BUTTON      SDLK_a
    #define B_BUTTON      SDLK_b
#endif

#define SELECT_BUTTON SDLK_ESCAPE
#define START_BUTTON  SDLK_RETURN

#define UP_BUTTON     SDLK_UP
#define DOWN_BUTTON   SDLK_DOWN
#define LEFT_BUTTON   SDLK_LEFT
#define RIGHT_BUTTON  SDLK_RIGHT

#define PAUSE_BUTTON  SDLK_PAUSE

#include <SDL.h>
#include <SDL_image.h>
#include <string>
#include <iostream>

#include "log.h"
#include "mic.cpp"
#include "timefmt.h"

#define WIDTH  320
#define HEIGHT 240
#define DEPTH  8

#ifdef MIPSEL
    #include <dirent.h>
    #include <sys/types.h>
    #include <sys/stat.h>
    #include <cstdlib>

    std::string BASE_PATH = std::string(std::getenv("HOME")) + "/.voice/";
#else
    std::string BASE_PATH = "./";
#endif

SDL_Surface *screen,
            *rec_btn,
            *rec_btn_ds,
            *stop_btn,
            *play_btn,
            *play_btn_ds,
            *pause_btn;

bool is_recording = false,
     is_playing = false;

long current_volume = 100;

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
        log(ERROR, "SDL_BlitSurface failed. %s.", SDL_GetError());
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
        log(ERROR, "Cannot load image. %s.", IMG_GetError());
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
std::string get_new_filename()
{
    return BASE_PATH + "rec_" + current_time() + ".wav";
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
                            current_file = get_new_filename();
                            log(INFO, "Recoding file: %s.", current_file.c_str());
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
                            log(INFO, "Playing file: %s.", current_file.c_str());
                            pmic->play(current_file);
                        }

                        is_playing = !is_playing;
                        redraw_buttons();
                    break;
                    case UP_BUTTON:
                        if (current_volume < 100)
                        {
                            current_volume++;
                            pmic->set_speaker_volume(current_volume);
                            log(INFO, "Volume changed to %ld.", current_volume);
                        }
                    break;
                    case DOWN_BUTTON:
                        if (current_volume > 0)
                        {
                            current_volume--;
                            pmic->set_speaker_volume(current_volume);
                            log(INFO, "Volume changed to %ld.", current_volume);
                        }
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

void on_terminate_exec()
{
    is_playing = false;
    is_recording = false;
    redraw_buttons();
    pmic->stop();
}

int main()
{
#ifdef MIPSEL
    DIR *dir = opendir(BASE_PATH.c_str());

    if (dir)
    {
        closedir(dir);
    }
    else
    {
        if (mkdir(BASE_PATH.c_str(), 0777) != 0)
        {
            log(FATAL, "Cannot create application directory.");
        }
    }
#endif

    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        log(FATAL, "Cannot init SDL.");
    }

    if (!(screen = SDL_SetVideoMode(WIDTH, HEIGHT, DEPTH, SDL_HWSURFACE | SDL_DOUBLEBUF)))
    {
        SDL_Quit();
        log(FATAL, "Cannot SetVideoMode.");
    }

    SDL_ShowCursor(SDL_DISABLE);

    int img_flags = IMG_INIT_PNG;

    if(!(IMG_Init(img_flags) & img_flags))
    {
        SDL_Quit();
        log(FATAL, "SDL_image could not initialize. %s.", IMG_GetError());
    }

    load_resources();
    redraw_buttons();

    pmic = new Mic();
    pmic->set_on_terminate_event(on_terminate_exec);
    pmic->set_speaker_volume(current_volume);

    main_loop();

    delete pmic;

    IMG_Quit();
    SDL_Quit();

    return 0;
}
