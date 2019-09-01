#pragma once

#include <vector>

#include "Globals.hpp"
#include "ITheme.hpp"
#include "ThemeProvider.hpp"

namespace Graphics
{
	void InitTheme(const std::string &fileName);
	void Blit(SDL_Surface *src, SDL_Surface *dest, const SDL_Rect *srcPos = nullptr, SDL_Rect *destPos = nullptr);
	void DrawText(const std::string &string, const int x, const int y, const SDL_Color colour, const bool blitToFileBase, const bool newDraw);
	void DrawPlaying(const bool paused);
	void DrawSelection();
	bool DrawMessageBox(const std::string &title, const std::string &caption1, const std::string &caption2);
	void Render();
}