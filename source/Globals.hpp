#pragma once

#ifdef __GNUC__
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_ttf.h>
#else
#include "SDL.h"
#include "SDL_mixer.h"
#include "SDL_ttf.h"
#endif

#ifndef _WIN32
#include <switch.h>
#else
#define KEY_PLUS SDLK_ESCAPE

#define KEY_A SDLK_a
#define KEY_B SDLK_b
#define KEY_X SDLK_x
#define KEY_Y SDLK_y

#define KEY_UP SDLK_UP
#define KEY_DOWN SDLK_DOWN
#define KEY_LEFT SDLK_LEFT
#define KEY_RIGHT SDLK_RIGHT

#define KEY_ZL SDLK_LEFTBRACKET
#define KEY_ZR SDLK_RIGHTBRACKET
#endif

#include <filesystem>
#include <memory>
#include <vector>

#include "ITheme.hpp"

#include "Formats/VGMStreamHandler.hpp"
#include "Formats/DspFile.hpp"
#include "Formats/DtkFile.hpp"
#include "Formats/GMEHandler.hpp"

enum PlayMode
{
	PlayOne,
	PlayAll,
	LoopOne,
	LoopAll
};

enum PlayStatus
{
	Stopped,
	Paused,
	Playing,
	Finished
};

constexpr uint8_t operator ""_pct(const unsigned long long percent)
{
	return static_cast<uint8_t>((static_cast<float>(percent) / 100.f) * 255.f);
};

extern std::unique_ptr<ITheme> gTheme;

extern SDL_Renderer *gRenderer;
extern SDL_Surface *gSurface, *gAlbumArt, *gFileBase, *gBase;
extern SDL_Window *gWindow;

extern TTF_Font *gFont;

extern uint32_t gSelection;
extern uint32_t gPlayState;

extern bool gGoPrevious;
extern bool gGoNext;

extern SDL_Event gEvent;

extern bool checkKey(
#ifndef _WIN32
	int k,
	uint64_t target
#else
	SDL_Event event,
	SDL_Keycode k
#endif
);

#ifdef _WIN32
extern bool gHasPerformed;
#endif