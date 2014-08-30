#include "screen.h"

SDL_Surface *screen = NULL,
            *rec_btn = NULL,
            *rec_btn_ds = NULL,
            *stop_btn = NULL,
            *play_btn = NULL,
            *play_btn_ds = NULL,
            *pause_btn = NULL,
            *vol_1 = NULL,
            *vol_2 = NULL,
            *vol_3 = NULL,
            *vol_mute = NULL,
            *vol_out = NULL;

TTF_Font *font_10 = NULL,
         *font_28 = NULL;

bool is_recording = false,
     is_playing   = false;

long current_volume = 100;

Mic   *pmic;
Mixer *pmixer;

std::string current_file;


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
    SDL_Surface *source = (is_recording || current_file.empty()) ? play_btn_ds : (is_playing ? pause_btn : play_btn);
    apply_surface(source, 162, 120, 32, 32);
}

void draw_buttons()
{
    draw_rec_stop();
    draw_play_pause();
}

void draw_timer(signed int hours, signed int minutes, signed int seconds)
{
    SDL_Color color = { 255, 255, 255 };
    char buffer[20];
    int w, h;

    std::sprintf(buffer, "%02d:%02d:%02d", hours, minutes, seconds);

    if(TTF_SizeText(font_28, buffer, &w, &h))
    {
        log(ERROR, "TTF_SizeText failed. %s.", TTF_GetError());
        w = 180;
    }

    SDL_Surface * s = TTF_RenderText_Solid(font_28, buffer, color);

    apply_surface(s, (WIDTH / 2) - (w / 2), 20);
}

void draw_volume()
{
    SDL_Surface *source;

    if (current_volume > 75)
    {
        source = vol_3;
    }
    else if (current_volume > 45)
    {
        source = vol_2;
    }
    else if (current_volume > 0)
    {
        source = vol_1;
    }
    else if (current_volume == 0)
    {
        source = vol_mute;
    }
    else
    {
        source = vol_out;
    }

    apply_surface(source, 4, 206, 32, 32);

    SDL_Color color = { 255, 255, 255 };

    std::string vol = to_string(current_volume);

    SDL_Surface * s = TTF_RenderText_Solid(font_10, vol.c_str(), color);

    apply_surface(s, 5, 216);
}

void draw_vu(signed int perc, int direction)
{
    Uint32 color;

    SDL_Rect tmp;
    tmp.x = (WIDTH / 2);
    tmp.y = 100;
    tmp.w = 4;
    tmp.h = 10;

    if (perc < 1)
    {
        perc = 1;
    }
    else if (perc > 100)
    {
        perc = 100;
    }

    int max = (perc / 5);

    for (int i = 0; i < 20; ++i)
    {
        if (i > max || max == 0)
        {
            color = SDL_MapRGB(screen->format, 20, 20, 20);
        }
        else if (i < 8)
        {
            color = SDL_MapRGB(screen->format, 0, 255, 0);
        }
        else if (i < 15)
        {
            color = SDL_MapRGB(screen->format, 255, 255, 0);
        }
        else
        {
            color = SDL_MapRGB(screen->format, 255, 0, 0);
        }

        if (i == 0)
        {
            tmp.x += 4 * direction;
        }
        else
        {
            tmp.x += 6 * direction;
        }

        SDL_FillRect(screen, &tmp, color);
    }

    SDL_Flip(screen);
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
    vol_1       = load_png("resources/32/Volume_1.png");
    vol_2       = load_png("resources/32/Volume_2.png");
    vol_3       = load_png("resources/32/Volume_3.png");
    vol_mute    = load_png("resources/32/Volume_Mute.png");
    vol_out     = load_png("resources/32/Volume_Not_Running.png");

    font_10 = TTF_OpenFont("resources/font/UbuntuMono-R.ttf", 10);
    font_28 = TTF_OpenFont("resources/font/Ubuntu-L.ttf", 48);

    if (!font_10 || !font_28)
    {
        log(FATAL, "Cannot load 'UbuntuMono-R.ttf'.");
    }
}

std::string get_new_filename()
{
    return BASE_PATH + "rec_" + current_time() + ".wav";
}

void main_loop()
{
    SDL_Event event;
    bool quit = false;

    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

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
                            pmixer->set_direction(CAPTURE);
                            pmic->record(current_file);
                        }

                        is_recording = !is_recording;
                        draw_buttons();
                    break;
                    case B_BUTTON:
                        if (is_recording || current_file.empty()) continue;

                        if (is_playing)
                        {
                            pmic->stop();
                        }
                        else
                        {
                            pmixer->set_direction(PLAYBACK);
                            pmic->play(current_file);
                        }

                        is_playing = !is_playing;
                        draw_buttons();
                    break;
                    case UP_BUTTON:
                        if (current_volume < 100)
                        {
                            current_volume++;
                            draw_volume();
                            pmixer->set_speaker_volume(current_volume);
                            log(INFO, "Volume changed to %ld.", current_volume);
                        }
                    break;
                    case DOWN_BUTTON:
                        if (current_volume > 0)
                        {
                            current_volume--;
                            draw_volume();
                            pmixer->set_speaker_volume(current_volume);
                            log(INFO, "Volume changed to %ld.", current_volume);
                        }
                    break;
                    default:
                        log(INFO, "Invalid key.");
                    break;
                }
            }
            break;
            case SDL_QUIT: /* window close */
                quit = true;
            break;
            default:
                //log(INFO, "Invalid key.");
            break;
        }
    }
}

void on_terminate_exec()
{
    is_playing = false;
    is_recording = false;
    draw_buttons();
    draw_vu(1, -1);
    draw_vu(1, 1);
    pmic->stop();
}

void on_vu_changed(signed int perc, signed int max_perc)
{
    draw_vu(perc, -1);
    draw_vu(perc, 1);
}

void on_time_changed(signed long total_seconds)
{
    int seconds = 0, minutes = 0, hours = 0, rest = 0;

    rest    = total_seconds;
    hours   = rest / 3600;
    rest    = rest - (hours * 3600);
    minutes = rest / 60;
    rest    = rest - (minutes * 60);
    seconds = rest;

    draw_timer(hours, minutes, seconds);
}

int main()
{
    create_app_dir();

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

    if (TTF_Init() < 0)
    {
        SDL_Quit();
        log(FATAL, "Unable to start the TTF.");
    }

    int img_flags = IMG_INIT_PNG;

    if(!(IMG_Init(img_flags) & img_flags))
    {
        TTF_Quit();
        SDL_Quit();
        log(FATAL, "SDL_image could not initialize. %s.", IMG_GetError());
    }

    pmixer = new Mixer();
    current_volume = pmixer->get_speaker_volume();
    pmixer->set_speaker_volume(current_volume);

    load_resources();
    draw_buttons();
    draw_timer(0, 0, 0);
    draw_volume();
    draw_vu(1, -1);
    draw_vu(1, 1);

    pmic = new Mic();
    pmic->set_on_terminate_event(on_terminate_exec);
    pmic->set_on_vu_change_event(on_vu_changed);


    main_loop();

    delete pmixer;
    delete pmic;

    TTF_CloseFont(font_10);

    IMG_Quit();
    TTF_Quit();
    SDL_Quit();

    return 0;
}
