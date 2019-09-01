#pragma once

#include <array>
#include <fstream>
#include <vector>

#include <cstring>

#include "../Codecs/DtkAdpcm.hpp"
#include "../IAudio.hpp"

class DtkFile
	: public IAudio
{
public:
	DtkFile() {};
	DtkFile(const std::string &fileName);

	virtual ~DtkFile() {};

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
	DtkAdpcmDecoder mDecoder{};
	size_t mOffset = 0;

	const std::string mFormatName = "Nintendo DTK ADPCM";

	int mSampleRate;
	int mNumChannels;

	int mNumSamples;

	bool mIsLooped;
	int mLoopStart;
	int mLoopEnd;

	size_t mBufferSize = 28 * 64;

	bool mIsBufferDone = false;

	std::vector<char> mInBuffer;
	std::vector<short> mOutBuffer;
};
