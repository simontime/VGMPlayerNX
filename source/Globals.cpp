#include "Globals.hpp"

std::unique_ptr<ITheme> gTheme;

SDL_Renderer *gRenderer;
SDL_Surface *gSurface, *gAlbumArt, *gFileBase, *gBase;
SDL_Window *gWindow;

TTF_Font *gFont;

uint32_t gSelection = 0;
uint32_t gPlayState = PlayOne;

bool gGoPrevious = false;
bool gGoNext = false;

SDL_Event gEvent;

#ifdef _WIN32
bool gHasPerformed = false;
#endif

bool checkKey(
#ifndef _WIN32
	int k,
	uint64_t target
#else
	SDL_Event event,
	SDL_Keycode k
#endif
)
{
#ifndef _WIN32
	return k & target;
#else
	return
		event.type == SDL_KEYDOWN &&
		event.key.keysym.sym == k &&
		!gHasPerformed;
#endif
}