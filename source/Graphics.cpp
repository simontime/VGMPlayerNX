#include "Graphics.hpp"

namespace Graphics
{
	void InitTheme(const std::string &fileName)
	{
		SDL_Rect albumRect = { 80, 80 };

		gTheme = std::make_unique<ThemeProvider>(fileName);

		auto bgRW  = SDL_RWFromMem((void *)gTheme->GetBackground().data(), gTheme->GetBackground().size());
		auto artRW = SDL_RWFromMem((void *)gTheme->GetAlbumArt().data(), gTheme->GetAlbumArt().size());

		gSurface  = SDL_LoadBMP_RW(bgRW,  SDL_TRUE);
		gAlbumArt = SDL_LoadBMP_RW(artRW, SDL_TRUE);

		gBase     = SDL_CreateRGBSurface(0, 1280, 720, 24, 255, 255, 255, 0);
		gFileBase = SDL_CreateRGBSurface(0, 1280, 720, 24, 255, 255, 255, 0);

		Blit(gAlbumArt, gSurface, nullptr, &albumRect);
		Blit(gSurface, gBase);
		Blit(gSurface, gFileBase);

		auto fontRW  = SDL_RWFromMem((void *)gTheme->GetFont().data(), gTheme->GetFont().size());
		gFont        = TTF_OpenFontRW(fontRW, SDL_TRUE, 28);
	}

	void Blit(SDL_Surface *src, SDL_Surface *dest, const SDL_Rect *srcPos, SDL_Rect *destPos)
	{
		SDL_BlitSurface(src, srcPos, dest, destPos);
	}

	void DrawText(const std::string &string, const int x, const int y, const SDL_Color colour, const bool blitToFileBase, const bool notFirstDraw)
	{
		SDL_Rect fontRect{ x, y };

		auto textSurface = TTF_RenderText_Blended(gFont, string.c_str(), colour);

		Blit(textSurface, gSurface, nullptr, &fontRect);

		if (blitToFileBase)
		{
			if (!notFirstDraw)
				Blit(gBase, gFileBase);

			Blit(textSurface, gFileBase, nullptr, &fontRect);
		}

		SDL_FreeSurface(textSurface);
	}

	void DrawPlaying(const bool paused)
	{
#ifndef _WIN32
		appletSetMediaPlaybackState(!paused);
#endif

		Blit(gFileBase, gSurface);

		DrawText(paused ? "Paused..." : "Playing...", 100, 552, { 255, 255, 255 }, false, true);
		DrawText("A: Play/Pause, B: Stop", 100, 592, { 255, 255, 255 }, false, true);
		DrawText(">", 566, 96 + ((gSelection % 16) * 33), { 255, 255, 255 }, false, true);

		Render();
	}

	void DrawSelection()
	{
#ifndef _WIN32
		appletSetMediaPlaybackState(false);
#endif

		std::string toggleText;
		
		switch (gPlayState)
		{
			case PlayOne: toggleText = "(X) Play one track";  break;
			case PlayAll: toggleText = "(X) Play all tracks"; break;
			case LoopOne: toggleText = "(X) Loop one track";  break;
			case LoopAll: toggleText = "(X) Loop all tracks"; break;
		}

		Blit(gFileBase, gSurface);

		DrawText("Waiting for selection...", 100, 552, { 255, 255, 255 }, false, true);
		DrawText("Up/Down: Select, A: Play", 100, 592, { 255, 255, 255 }, false, true);
		DrawText(">", 566, 96 + ((gSelection % 16) * 33), { 255, 255, 255 }, false, true);
		DrawText(toggleText, 80, 24, { 255, 255, 255 }, false, true);

		Render();
	}

	bool DrawMessageBox(const std::string &title, const std::string &caption1, const std::string &caption2)
	{
		SDL_Surface *original = SDL_CreateRGBSurface(0, 1280, 720, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);
		SDL_Surface *faded = SDL_CreateRGBSurface(0, 1280, 720, 32, 
#ifndef _WIN32 // Fixes glitches on Switch and PC - don't ask me why it occurs, I have no fucking idea.
			0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF
#else
			0, 0, 0, 0
#endif
		);
		SDL_Surface *mbox = SDL_CreateRGBSurface(0, 500, 250, 32, 0xFF000000, 0x00FF0000, 0x0000FF00, 0x000000FF);

		SDL_Rect box    = { 0,   0,   500, 250 };
		SDL_Rect button = { 150, 177, 200, 50  };
		SDL_Rect boxtop = { 0,   0,   500, 56  };
		SDL_Rect srcSrf = { 390, 0,   500, 250 };

		auto DrawTextLocal = [&](const std::string &string, const int x, const int y, const SDL_Color colour) {
			SDL_Rect fontRect{ x, y };
			auto textSurface = TTF_RenderText_Blended(gFont, string.c_str(), colour);
			Blit(textSurface, mbox, nullptr, &fontRect);
			SDL_FreeSurface(textSurface);
		};

		Blit(gSurface, original);
		Blit(original, gSurface);

		SDL_Texture *oTex = SDL_CreateTextureFromSurface(gRenderer, gSurface);

		for (int i = 0; i < 10; i++)
		{
			SDL_RenderCopy(gRenderer, oTex, nullptr, nullptr);
			SDL_SetRenderDrawColor(gRenderer, 0, 0, 0, i * 15);
			SDL_RenderFillRect(gRenderer, nullptr);
			SDL_RenderPresent(gRenderer);
			SDL_Delay(16);
		}

		SDL_DestroyTexture(oTex);

		SDL_RenderReadPixels(gRenderer, nullptr, 0, faded->pixels, faded->pitch);

		SDL_FillRect(mbox, &box,    SDL_MapRGBA(mbox->format, 100_pct, 100_pct, 100_pct, 80_pct));
		SDL_FillRect(mbox, &boxtop, SDL_MapRGBA(mbox->format, 90_pct,   90_pct,  90_pct, 80_pct));
		SDL_FillRect(mbox, &button, SDL_MapRGBA(mbox->format, 75_pct,   75_pct,  75_pct, 90_pct));

		DrawTextLocal(title,    20,  12,  { 0,      0,      0,      100_pct });
		DrawTextLocal(caption1, 20,  76,  { 40_pct, 40_pct, 40_pct, 100_pct });
		DrawTextLocal(caption2, 20,  116, { 40_pct, 40_pct, 40_pct, 100_pct });
		DrawTextLocal("OK",     228, 185, { 0,      0,      0,      100_pct });

		SDL_Texture *fadedTex = SDL_CreateTextureFromSurface(gRenderer, faded);
		SDL_Texture *mboxTex  = SDL_CreateTextureFromSurface(gRenderer, mbox);

		for (int i = -5; i < 5; i++)
		{
			srcSrf.y = 55 * i;
			SDL_RenderCopy(gRenderer, fadedTex, nullptr, nullptr);
			SDL_RenderCopy(gRenderer, mboxTex,  nullptr, &srcSrf);
			SDL_RenderPresent(gRenderer);
			SDL_Delay(16);
		}

		SDL_DestroyTexture(fadedTex);
		SDL_DestroyTexture(mboxTex);

		while (true)
		{
#ifndef _WIN32
			hidScanInput();
			uint64_t k = hidKeysDown(CONTROLLER_P1_AUTO);

			touchPosition tpos;
			hidTouchRead(&tpos, 0);
#else
			SDL_PollEvent(&gEvent);
			SDL_Event k = gEvent;
#endif

			auto renderFree = [&]() {
				Blit(original, gSurface);
				Render();

				SDL_FreeSurface(original);
				SDL_FreeSurface(faded);
				SDL_FreeSurface(mbox);
			};

			if (
				checkKey(k, KEY_A)
#ifndef _WIN32
				|| ((tpos.px >= 540 && tpos.px <= 740) 
				&&  (tpos.py >= 400 && tpos.py <= 450))
#endif
			)
			{
				renderFree();
				return true;
			}

			if (checkKey(k, KEY_B))
			{
				renderFree();
				return false;
			}

			SDL_Delay(1);
		}
	}

	void Render()
	{
		auto texture = SDL_CreateTextureFromSurface(gRenderer, gSurface);

		SDL_RenderCopy(gRenderer, texture, nullptr, nullptr);
		SDL_RenderPresent(gRenderer);

		SDL_DestroyTexture(texture);
	}
}