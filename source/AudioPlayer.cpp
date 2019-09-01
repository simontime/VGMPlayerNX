#include "AudioPlayer.hpp"

namespace Audio
{
	static Uint8 *audioChunk;
	static Uint8 *audioPos;

	static int audioLen;

	static const PlayStatus ButtonPressCallback()
	{
#ifndef _WIN32
		hidScanInput();
		uint64_t k = hidKeysDown(CONTROLLER_P1_AUTO);
#else
		SDL_PollEvent(&gEvent);
		SDL_Event k = gEvent;

		if (k.type == SDL_KEYUP)
			gHasPerformed = false;
#endif

		if (checkKey(k, KEY_A))
		{
			if (!Mix_PausedMusic())
			{
				Mix_PauseMusic();
				Graphics::DrawPlaying(true);
			}
			else
			{
				Mix_ResumeMusic();
				Graphics::DrawPlaying(false);
			}

			return Paused;
		}

		if (checkKey(k, KEY_B))
		{
			Mix_HaltMusic();
			return Stopped;
		}

		if (checkKey(k, KEY_ZL))
		{
			Mix_HaltMusic();
			gGoPrevious = true;
			return Stopped;
		}

		if (checkKey(k, KEY_ZR))
		{
			Mix_HaltMusic();
			gGoNext = true;
			return Stopped;
		}

#ifdef _WIN32
		if (k.type == SDL_KEYDOWN)
			gHasPerformed = true;
#endif

		return Playing;
	}

	static const PlayStatus ButtonPressCallbackBuffer()
	{
#ifndef _WIN32
		hidScanInput();
		uint64_t k = hidKeysDown(CONTROLLER_P1_AUTO);
#else
		SDL_PollEvent(&gEvent);
		SDL_Event k = gEvent;

		if (k.type == SDL_KEYUP)
			gHasPerformed = false;
#endif

		if (checkKey(k, KEY_A))
		{
			if (SDL_GetAudioStatus() == SDL_AUDIO_PLAYING)
			{
				SDL_PauseAudio(1);
				Graphics::DrawPlaying(true);
			}
			else
			{
				SDL_PauseAudio(0);
				Graphics::DrawPlaying(false);
			}

			return Paused;
		}

		if (checkKey(k, KEY_B))
		{
			SDL_CloseAudio();
			return Stopped;
		}

		if (checkKey(k, KEY_ZL))
		{
			SDL_CloseAudio();
			gGoPrevious = true;
			return Stopped;
		}

		if (checkKey(k, KEY_ZR))
		{
			SDL_CloseAudio();
			gGoNext = true;
			return Stopped;
		}

#ifdef _WIN32
		if (k.type == SDL_KEYDOWN)
			gHasPerformed = true;
#endif

		return Playing;
	}

	void FillCallback(void *udata, Uint8 *stream, int len)
	{
		SDL_memset(stream, 0, len);

		if (audioLen == 0)
			return;

		len = (len > audioLen ? audioLen : len);

		SDL_MixAudio(stream, audioPos, len, SDL_MIX_MAXVOLUME);

		audioPos += len;
		audioLen -= len;
	}

	static const PlayStatus PlaySong(IAudio &audio, const bool loop, const PlayStatus(*cb)())
	{
		auto original = SDL_DuplicateSurface(gFileBase);

		Graphics::DrawText(audio.GetFormatName() + " | " +
			std::to_string(audio.GetSampleRate()) + " Hz | " +
			std::to_string(audio.GetNumChannels()) + " ch",
			80, 24, { 255, 255, 255 }, true, true);

		Graphics::Render();

		int dataCount = 0;

		SDL_AudioSpec spec{};

		spec.freq     = audio.GetSampleRate();
		spec.format   = AUDIO_S16LSB;
		spec.channels = audio.GetNumChannels();
		spec.samples  = static_cast<Uint16>(audio.GetBufferSize());
		spec.callback = FillCallback;

		SDL_OpenAudio(&spec, NULL);
		SDL_PauseAudio(0);

StartPlaying:
		while (!audio.GetIsBufferDone())
		{
			auto &data = audio.GetBuffer();
			dataCount += data.size() * audio.GetNumChannels();
			audioLen = data.size() * audio.GetNumChannels();
			audioPos = audioChunk = (Uint8 *)data.data();

			while (audioLen > 0)
			{
				if (cb() == Stopped)
				{
					Graphics::Blit(original, gFileBase);
					SDL_FreeSurface(original);
					return Finished;
				}

				SDL_Delay(1);
			}
		}

		if (!audio.GetIsLooped() && loop)
		{
			audio.ResetState();
			goto StartPlaying;
		}

		SDL_CloseAudio();

		Graphics::Blit(original, gFileBase);
		SDL_FreeSurface(original);

		return Stopped;
	}

	static const PlayStatus PlaySong(const std::filesystem::path &path, const bool loop, const PlayStatus(*cb)())
	{
		Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 4096);

		auto music = Mix_LoadMUS(path.string().c_str());
		Mix_PlayMusic(music, loop ? -1 : 0);

		while (Mix_PlayingMusic())
		{
			if (cb() == Stopped)
				return Finished;

			SDL_Delay(1);
		}

		Mix_FreeMusic(music);

		Mix_CloseAudio();

		return Stopped;
	}

	PlayStatus Play(const std::filesystem::path &path, const bool loop)
	{
		PlayStatus playStatus;
		bool suffixFound = false;

		gme_type_t type;

		Graphics::Blit(gBase, gSurface);
		Graphics::DrawPlaying(false);

		if (gme_identify_file(path.string().c_str(), &type), type)
		{
			auto gme = new GMEHandler(path.string());
			playStatus = Audio::PlaySong(*gme, loop, Audio::ButtonPressCallbackBuffer);
			delete gme;
		}
		else if (init_vgmstream(path.string().c_str()))
		{
			auto vgm = new VGMStreamHandler(path.string());
			playStatus = Audio::PlaySong(*vgm, loop, Audio::ButtonPressCallbackBuffer);
			delete vgm;
		}
		else
		{
			playStatus = Audio::PlaySong(path, loop, Audio::ButtonPressCallback);
		}

		Graphics::DrawSelection();

		return playStatus;
	}
}