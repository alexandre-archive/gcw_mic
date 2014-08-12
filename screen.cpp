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

#include <SDL.h>
#include <SDL_image.h>
#include <string>

using namespace std;

#define WIDTH  320
#define HEIGHT 240
#define DEPTH  8

SDL_Surface *screen;

void main_loop()
{

    SDL_Event event;
    bool quit = false;

    while(!quit)
    {
        while(SDL_PollEvent(&event))
        {
            switch(event.type)
            {
                /* Keyboard event */
                /* Pass the event data onto PrintKeyInfo() */
                //case SDL_KEYDOWN:
                case SDL_KEYUP:
                    //PrintKeyInfo(&event.key);
                    //SDLKey keyPressed = event.key.keysym.sym;

                    switch (event.key.keysym.sym/*keyPressed*/)
                    {
                        case SDLK_ESCAPE:
                            quit = true;
                        break;
                        default:
                        break;
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
}
/*
SDL_Surface* loadSurface( string path )
{
    //The final optimized image 
    SDL_Surface* optimizedSurface = NULL;
    //Load image at specified path
    SDL_Surface* loadedSurface = IMG_Load( path.c_str() );

    if( loadedSurface == NULL )
    {
        printf( "Unable to load image %s! SDL_image Error: %s\n", path.c_str(), IMG_GetError() );
    }
    else
    {
        //Convert surface to screen format 
        optimizedSurface = SDL_ConvertSurface( loadedSurface, screen->format, NULL );

        if( optimizedSurface == NULL )
        {
            printf( "Unable to optimize image %s! SDL Error: %s\n", path.c_str(), SDL_GetError() );
        }

        //Get rid of old loaded surface 

        SDL_FreeSurface( loadedSurface );
    }

    return optimizedSurface;
}
*/
SDL_Surface* load_png(string path)
{
    SDL_Surface *image = IMG_Load(path.c_str());

    if(!image)
    {
        printf("IMG_Load: %s\n", IMG_GetError());
    }

    return image;
}

void apply_surface(SDL_Surface* image, int x, int y)
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

    SDL_Surface* rec_btn = load_png("resources/32/player_record.png");
    SDL_Surface* stop_btn = load_png("resources/32/player_stop.png");
    SDL_Surface* play_btn = load_png("resources/32/player_play.png");
    SDL_Surface* pause_btn = load_png("resources/32/player_pause.png");

    apply_surface(rec_btn, 126, 120);
    apply_surface(play_btn, 162, 120);

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