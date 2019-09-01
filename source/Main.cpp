#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "AudioPlayer.hpp"
#include "Formats/DspFile.hpp"
#include "Globals.hpp"
#include "Graphics.hpp"
#include "ITheme.hpp"
#include "ThemeProvider.hpp"
#include "Utils.hpp"

// TO-DO: Clean this messy function up.

int main(int argc, char *argv[])
{
	SDL_Init(SDL_INIT_AUDIO | SDL_INIT_VIDEO);
	TTF_Init();

#ifndef _WIN32
	romfsMount("data");
#endif

	gWindow = SDL_CreateWindow("ThemePlayer",
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, SDL_WINDOW_OPENGL);
	gRenderer = SDL_CreateRenderer(gWindow, -1, 
#ifdef _WIN32
		SDL_RENDERER_SOFTWARE
#else
		SDL_RENDERER_ACCELERATED
#endif
	);

	SDL_SetRenderDrawBlendMode(gRenderer, SDL_BLENDMODE_BLEND);

	std::string location;
	std::string themeLocation =
#ifndef _WIN32
		"data:/"
#endif
		"default.tpk";

#ifdef _WIN32
	location = std::filesystem::current_path().string() + "/music";
#else
	location = "sdmc:/";
#endif

	std::vector<std::filesystem::path> audioFiles;

	Graphics::InitTheme(themeLocation);

	auto populateDirs = [&]() {
		audioFiles = Utils::File::GetAllInDirectory(location);
		std::sort(audioFiles.begin(), audioFiles.end());
	};

	auto drawDirs = [&]() {
		for (uint32_t i = 0;
			i < (((audioFiles.size() - gSelection) < 16) 
				? (audioFiles.size() - gSelection) : 16); i++)
		{
			std::string filename;

			try
			{
				if (std::filesystem::is_directory(audioFiles[i + gSelection]))
					filename = '/';
				
				filename += audioFiles[i + gSelection].filename().string();

				if (filename.length() > 40)
					filename = filename.substr(0, 40) + "...";
			}
			catch (...) {};

			Graphics::DrawText(filename, 584, 96 + (i * 33), { 255, 255, 255 }, true, i);
		}

		Graphics::DrawSelection();
	};

	populateDirs();
	drawDirs();

#ifdef _WIN32
	SDL_Event event;
#endif

	while (true)
	{
		auto stateSwitch = [&]() {
			switch (gPlayState)
			{
				case PlayOne:
				{
					Audio::Play(audioFiles[gSelection], false);
					break;
				}
				case PlayAll:
				{
					for (; gSelection < audioFiles.size(); gSelection++)
					{
						if (Audio::Play(audioFiles[gSelection], false) == Finished)
							break;
					}
					break;
				}
				case LoopOne:
				{
					Audio::Play(audioFiles[gSelection], true);
					break;
				}
				case LoopAll:
				{
					while (true)
					{
						if (Audio::Play(audioFiles[gSelection], false) == Finished)
							break;

						if (gSelection++ == audioFiles.size() - 1)
						{
							gSelection = 0;
							drawDirs();
						}
					}
					break;
				}
			}
		};

		auto fileSelect = [&]() {
			if (std::filesystem::is_directory(audioFiles[gSelection]))
			{
				if (!std::filesystem::is_empty(audioFiles[gSelection]))
				{
					location = audioFiles[gSelection].string();
					gSelection = 0;
					populateDirs();
					drawDirs();
				}
				else
					Graphics::DrawMessageBox("An error occurred:",
						audioFiles[gSelection].filename().string(), "Directory empty.");
			}
			else if (audioFiles[gSelection].extension() == ".tpk")
			{
				themeLocation = audioFiles[gSelection].string();
				Graphics::InitTheme(themeLocation);
				gSelection = 0;
				drawDirs();
			}
			else
				stateSwitch();
		};

#ifndef _WIN32
		hidScanInput();
		uint64_t k = hidKeysDown(CONTROLLER_P1_AUTO);
#else
		SDL_PollEvent(&event);
		SDL_Event k = event;

		if (event.type == SDL_KEYUP)
			gHasPerformed = false;
#endif

		if (gGoPrevious)
		{
			gGoPrevious = false;

			if (gSelection > 0)
			{
				if (gSelection-- % 16 == 0)
				{
					gSelection -= 15;
					drawDirs();
					gSelection += 15;
				}

				fileSelect();
			}

			Graphics::DrawSelection();
		}

		if (gGoNext)
		{
			gGoNext = false;

			if (gSelection < audioFiles.size() - 1)
			{
				if (++gSelection % 16 == 0)
					drawDirs();

				fileSelect();
			}

			Graphics::DrawSelection();
		}

		if (checkKey(k, KEY_UP) && gSelection > 0)
		{
			if (gSelection-- % 16 == 0)
			{
				gSelection -= 15;
				drawDirs();
				gSelection += 15;
			}

			Graphics::DrawSelection();
		}

		if (checkKey(k, KEY_DOWN) && gSelection < audioFiles.size() - 1)
		{
			if (++gSelection % 16 == 0)
				drawDirs();
			else
				Graphics::DrawSelection();
		}

		if (checkKey(k, KEY_A))
		{
			fileSelect();
		}

		if (checkKey(k, KEY_B))
		{
			if (audioFiles[gSelection].parent_path().has_parent_path())
				location = audioFiles[gSelection].parent_path().parent_path().string();

#ifndef _WIN32 // Switch std::filesystem bug.
			if (location == "sdmc:")
				location += '/';
#endif

			gSelection = 0;
			populateDirs();
			drawDirs();
		}

		if (checkKey(k, KEY_X))
		{
			// Hmm... today I will make a state machine
			gPlayState = gPlayState == LoopAll ? PlayOne : gPlayState + 1;
			
			Graphics::Blit(gBase, gSurface);
			Graphics::DrawSelection();
		}

		if (checkKey(k, KEY_PLUS))
		{
			if (Graphics::DrawMessageBox("Would you like to exit?", "A: OK", "B: Go back"))
				break;
		}

#ifdef _WIN32
		if (event.type == SDL_KEYDOWN)
			gHasPerformed = true;
#endif

		SDL_Delay(1);
	}

	TTF_CloseFont(gFont);
	TTF_Quit();

	SDL_FreeSurface(gSurface);
	SDL_FreeSurface(gBase);
	SDL_FreeSurface(gFileBase);

	SDL_DestroyRenderer(gRenderer);
	SDL_DestroyWindow(gWindow);

	SDL_Quit();

#ifndef _WIN32
	romfsUnmount("data");
#endif

	return 0;
}