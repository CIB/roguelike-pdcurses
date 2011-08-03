/* Public Domain Curses */

#include "pdcsdl.h"

RCSID("$Id: pdcscrn.c,v 1.34 2008/07/14 04:24:52 wmcbrine Exp $")

#include <stdlib.h>
#include "deffont.h"
#include "deficon.h"

SDL_Surface *pdc_screen = NULL, *pdc_font = NULL, *pdc_unscaled_font = NULL, 
			*pdc_icon = NULL, *pdc_back = NULL, *pdc_tileback = NULL, 
			*pdc_unscaled_tileset = NULL, *pdc_tileset = NULL;
int pdc_sheight = 0, pdc_swidth = 0, pdc_yoffset = 0, pdc_xoffset = 0;
static int is_fullscreen = 0, pdc_desktop_width = 0, pdc_desktop_height = 0;

SDL_Color pdc_color[16];
Uint32 pdc_mapped[16];
int pdc_fheight, pdc_fwidth, pdc_flastc;
bool pdc_own_screen;

// copypasted from http://www.libsdl.org/cgi/docwiki.cgi/Pixel_Access
Uint32 getpixel(SDL_Surface *surface, int x, int y)
{
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to retrieve */
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch(bpp) {
    case 1:
        return *p;
        break;

    case 2:
        return *(Uint16 *)p;
        break;

    case 3:
        if(SDL_BYTEORDER == SDL_BIG_ENDIAN)
            return p[0] << 16 | p[1] << 8 | p[2];
        else
            return p[0] | p[1] << 8 | p[2] << 16;
        break;

    case 4:
        return *(Uint32 *)p;
        break;

    default:
        return 0;       /* shouldn't happen, but avoids warnings */
    }
}
void putpixel(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
    int bpp = surface->format->BytesPerPixel;
    /* Here p is the address to the pixel we want to set */
    Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;

    switch(bpp) {
    case 1:
        *p = pixel;
        break;

    case 2:
        *(Uint16 *)p = pixel;
        break;

    case 3:
        if(SDL_BYTEORDER == SDL_BIG_ENDIAN) {
            p[0] = (pixel >> 16) & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = pixel & 0xff;
        } else {
            p[0] = pixel & 0xff;
            p[1] = (pixel >> 8) & 0xff;
            p[2] = (pixel >> 16) & 0xff;
        }
        break;

    case 4:
        *(Uint32 *)p = pixel;
        break;
    }
}

// copypasted from http://www.sdltutorials.com/sdl-scale-surface/
SDL_Surface* ScaleSurface(SDL_Surface *Surface, Uint16 Width, Uint16 Height)
{
    if(!Surface || !Width || !Height)
        return 0;
   
    SDL_Surface *_ret = SDL_CreateRGBSurface(Surface->flags, Width, Height, Surface->format->BitsPerPixel,
        Surface->format->Rmask, Surface->format->Gmask, Surface->format->Bmask, Surface->format->Amask);
	_ret->format->palette = Surface->format->palette;

    double  _stretch_factor_x = ( ((double) Width )  / ((double)Surface->w)),
        _stretch_factor_y = ( ((double) Height) / ( (double) Surface->h) );
	
	Sint32 y;
	Sint32 x;
	Sint32 o_y;
	Sint32 o_x;
    for(y = 0; y < Surface->h; y++)
        for(x = 0; x < Surface->w; x++)
            for(o_y = 0; o_y < _stretch_factor_y; ++o_y)
                for(o_x = 0; o_x < _stretch_factor_x; ++o_x)
                    putpixel(_ret, ( (Sint32) (_stretch_factor_x * x)) + o_x,
                        ((Sint32) (_stretch_factor_y * y)) + o_y, getpixel(Surface, x, y));

    return _ret;
}

/* COLOR_PAIR to attribute encoding table. */

static struct {short f, b;} atrtab[PDC_COLOR_PAIRS];

void PDC_retile(void)
{
    if (pdc_tileback)
        SDL_FreeSurface(pdc_tileback);

    pdc_tileback = SDL_DisplayFormat(pdc_screen);

    if (pdc_back)
    {
        SDL_Rect dest;

        dest.y = 0;

        while (dest.y < pdc_tileback->h)
        {
            dest.x = 0;

            while (dest.x < pdc_tileback->w)
            {
                SDL_BlitSurface(pdc_back, 0, pdc_tileback, &dest);
                dest.x += pdc_back->w;
            }

            dest.y += pdc_back->h;
        }

        SDL_BlitSurface(pdc_tileback, 0, pdc_screen, 0);
    }
}

void PDC_scr_close(void)
{
    PDC_LOG(("PDC_scr_close() - called\n"));
}

void PDC_scr_free(void)
{
    if (SP)
        free(SP);
}

/* open the physical screen -- allocate SP, miscellaneous intialization */

int PDC_scr_open(int argc, char **argv)
{
    int i;

    PDC_LOG(("PDC_scr_open() - called\n"));

    SP = calloc(1, sizeof(SCREEN));

    if (!SP)
        return ERR;

    pdc_own_screen = !pdc_screen;

    if (pdc_own_screen)
    {
        if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_TIMER) < 0)
        {
            fprintf(stderr, "Could not start SDL: %s\n", SDL_GetError());
            return ERR;
        }

        atexit(SDL_Quit);
    }
	

	const SDL_VideoInfo* info = SDL_GetVideoInfo();
	pdc_desktop_width = info->current_w;
	pdc_desktop_height = info->current_h;

    if (!pdc_font)
    {
        const char *fname = getenv("PDC_FONT");
        pdc_unscaled_font = SDL_LoadBMP(fname ? fname : "pdcfont.bmp");
		
		pdc_fwidth = pdc_swidth / 80;
		pdc_fheight = pdc_sheight / 25;
		if(!pdc_fwidth || !pdc_fheight) {
			pdc_fwidth = 12;
			pdc_fheight = 16;
		}
		pdc_font = ScaleSurface(pdc_unscaled_font, pdc_fwidth*16, pdc_fheight*16);
    }

    if (!pdc_font)
        pdc_font = SDL_LoadBMP_RW(SDL_RWFromMem(deffont, sizeof(deffont)), 0);

    if (!pdc_font)
    {
        fprintf(stderr, "Could not load font\n");
        return ERR;
    }
	
	if(!pdc_tileset) {
		pdc_unscaled_tileset = SDL_LoadBMP("tileset.bmp");
		if(pdc_unscaled_tileset) {
			double scale = ((double) pdc_fwidth * 16) / pdc_unscaled_tileset->w; 
			pdc_tileset = ScaleSurface(pdc_unscaled_tileset, pdc_fwidth * 16, (int) (scale * pdc_unscaled_tileset->h));
		}
	}
	if(!pdc_tileset) {
		fprintf(stderr, "Could not load tileset\n");
		// we can still go on without tileset
	} else {
		// configure the image in memory
		Uint32 colorkey = SDL_MapRGB(pdc_tileset->format, 0, 0, 0);
		SDL_SetColorKey(pdc_tileset, SDL_SRCCOLORKEY | SDL_RLEACCEL, colorkey);
	}
	

    SP->mono = !pdc_font->format->palette;

    if (!SP->mono && !pdc_back)
    {
        const char *bname = getenv("PDC_BACKGROUND");
        pdc_back = SDL_LoadBMP(bname ? bname : "pdcback.bmp");
    }

    if (!SP->mono && (pdc_back || !pdc_own_screen))
    {
        SP->orig_attr = TRUE;
        SP->orig_fore = COLOR_WHITE;
        SP->orig_back = -1;
    }
    else
        SP->orig_attr = FALSE;

    pdc_fheight = pdc_font->h / 16;
    pdc_fwidth = pdc_font->w / 16;

    if (!SP->mono)
        pdc_flastc = pdc_font->format->palette->ncolors - 1;

    if (pdc_own_screen && !pdc_icon)
    {
        const char *iname = getenv("PDC_ICON");
        pdc_icon = SDL_LoadBMP(iname ? iname : "pdcicon.bmp");

        if (!pdc_icon)
            pdc_icon = SDL_LoadBMP_RW(SDL_RWFromMem(deficon,
                                                    sizeof(deficon)), 0);

        if (pdc_icon)
            SDL_WM_SetIcon(pdc_icon, NULL);
    }

    if (pdc_own_screen)
    {
        const char *env = getenv("PDC_LINES");
        pdc_sheight = (env ? atoi(env) : 25) * pdc_fheight;

        env = getenv("PDC_COLS");
        pdc_swidth = (env ? atoi(env) : 80) * pdc_fwidth;

        pdc_screen = SDL_SetVideoMode(pdc_swidth, pdc_sheight, 0,
            SDL_SWSURFACE|SDL_ANYFORMAT|SDL_RESIZABLE);
    }
    else
    {
        if (!pdc_sheight)
            pdc_sheight = pdc_screen->h - pdc_yoffset;

        if (!pdc_swidth)
            pdc_swidth = pdc_screen->w - pdc_xoffset;
    }

    if (!pdc_screen)
    {
        fprintf(stderr, "Couldn't create a surface: %s\n", SDL_GetError());
        return ERR;
    }

    if (SP->orig_attr)
        PDC_retile();

    for (i = 0; i < 8; i++)
    {
        pdc_color[i].r = (i & COLOR_RED) ? 0xc0 : 0;
        pdc_color[i].g = (i & COLOR_GREEN) ? 0xc0 : 0;
        pdc_color[i].b = (i & COLOR_BLUE) ? 0xc0 : 0;

        pdc_color[i + 8].r = (i & COLOR_RED) ? 0xff : 0x40;
        pdc_color[i + 8].g = (i & COLOR_GREEN) ? 0xff : 0x40;
        pdc_color[i + 8].b = (i & COLOR_BLUE) ? 0xff : 0x40;
    }

    for (i = 0; i < 16; i++)
        pdc_mapped[i] = SDL_MapRGB(pdc_screen->format, pdc_color[i].r,
                                   pdc_color[i].g, pdc_color[i].b);

    SDL_EnableUNICODE(1);

    PDC_mouse_set();

    if (pdc_own_screen)
        PDC_set_title(argc ? argv[0] : "PDCurses");

    SP->lines = PDC_get_rows();
    SP->cols = PDC_get_columns();

    SP->mouse_wait = PDC_CLICK_PERIOD;
    SP->audible = FALSE;

    PDC_reset_prog_mode();

    return OK;
}

/* the core of resize_term() */

int PDC_resize_screen(int nlines, int ncols)
{
    if (!pdc_own_screen)
        return ERR;

    if (nlines && ncols)
    {
        pdc_sheight = nlines * pdc_fheight;
        pdc_swidth = ncols * pdc_fwidth;
    }

    SDL_FreeSurface(pdc_screen);

    pdc_screen = SDL_SetVideoMode(pdc_swidth, pdc_sheight, 0,
        SDL_SWSURFACE|SDL_ANYFORMAT|SDL_RESIZABLE);

    if (pdc_tileback)
        PDC_retile();

    SP->resized = FALSE;
    SP->cursrow = SP->curscol = 0;

    return OK;
}

int pdc_toggle_fullscreen() {
    SDL_FreeSurface(pdc_screen);

	// switch between fullscreen/windowed mode
	if(!is_fullscreen) {
	
		is_fullscreen = 1;
		pdc_swidth = pdc_screen->w;
		pdc_sheight = pdc_screen->h;
		
		pdc_screen = SDL_SetVideoMode(pdc_desktop_width, pdc_desktop_height, 0,
			SDL_SWSURFACE|SDL_ANYFORMAT|SDL_FULLSCREEN);
	} else {
		is_fullscreen = 0;
		pdc_screen = SDL_SetVideoMode(pdc_swidth, pdc_sheight, 0,
			SDL_SWSURFACE|SDL_ANYFORMAT);
	}
	
	// update the screen and the buffers
    if (pdc_tileback)
        PDC_retile();

	pdc_fwidth = pdc_screen->w / 80;
	pdc_fheight = pdc_fwidth * 4 / 3;
	free(pdc_font);
	pdc_font = ScaleSurface(pdc_unscaled_font, pdc_fwidth*16, pdc_fheight*16);
	
	if(pdc_unscaled_tileset) {
		double scale = ((double) pdc_fwidth * 16) / pdc_unscaled_tileset->w; 
		free(pdc_tileset);
		pdc_tileset = ScaleSurface(pdc_unscaled_tileset, pdc_fwidth * 16, (int) (scale * pdc_unscaled_tileset->h));
	}
	
	pdc_yoffset = (pdc_screen->h - pdc_fheight * 25) / 2;
	
	return 1;
}


void PDC_reset_prog_mode(void)
{
    PDC_LOG(("PDC_reset_prog_mode() - called.\n"));

    PDC_flushinp();
    SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);
}

void PDC_reset_shell_mode(void)
{
    PDC_LOG(("PDC_reset_shell_mode() - called.\n"));

    SDL_EnableKeyRepeat(0, 0);
    PDC_flushinp();
}

void PDC_restore_screen_mode(int i)
{
}

void PDC_save_screen_mode(int i)
{
}

void PDC_init_pair(short pair, short fg, short bg)
{
    atrtab[pair].f = fg;
    atrtab[pair].b = bg;
}

int PDC_pair_content(short pair, short *fg, short *bg)
{
    *fg = atrtab[pair].f;
    *bg = atrtab[pair].b;

    return OK;
}

bool PDC_can_change_color(void)
{
    return TRUE;
}

int PDC_color_content(short color, short *red, short *green, short *blue)
{
    *red = DIVROUND(pdc_color[color].r * 1000, 255);
    *green = DIVROUND(pdc_color[color].g * 1000, 255);
    *blue = DIVROUND(pdc_color[color].b * 1000, 255);

    return OK;
}

int PDC_init_color(short color, short red, short green, short blue)
{
    pdc_color[color].r = DIVROUND(red * 255, 1000);
    pdc_color[color].g = DIVROUND(green * 255, 1000);
    pdc_color[color].b = DIVROUND(blue * 255, 1000);

    pdc_mapped[color] = SDL_MapRGB(pdc_screen->format, pdc_color[color].r,
                                   pdc_color[color].g, pdc_color[color].b);

    wrefresh(curscr);

    return OK;
}
