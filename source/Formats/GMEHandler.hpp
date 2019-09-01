#pragma once

#include <array>
#include <fstream>
#include <vector>

extern "C"
{
#include "../GME/gme.h"
};

#include "../IAudio.hpp"

class GMEHandler
	: public IAudio
{
public:
	GMEHandler() {};
	GMEHandler(const std::string &fileName);

	virtual ~GMEHandler();

	const std::string &GetFormatName() const;

	const int GetSampleRate() const;
	const int GetNumChannels() const;

	const int GetNumSamples() const;

	const bool GetIsLooped() const;
	const int GetLoopStart() const;
	const int GetLoopEnd() const;

	const size_t GetBufferSize() const;
	const bool GetIsBufferDone() const;

	const std::vector<short> &GetBuffer();

	void ResetState();

private:
	Music_Emu *emu;

	std::string mFormatName;

	int mSampleRate;
	int mNumChannels;

	int mNumSamples;

	bool mIsLooped;
	int mLoopStart;
	int mLoopEnd;

	size_t mBufferSize = 2048;

	bool mIsBufferDone = false;

	std::vector<short> mOutBuffer;
};
