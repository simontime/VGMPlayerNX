#pragma once

#include <array>
#include <fstream>
#include <vector>

#include <cstring>

#include "../Codecs/DspAdpcm.hpp"
#include "../IAudio.hpp"

struct DspHeader
{
	uint32_t mSampleCount;
	uint32_t mNibbleCount;
	uint32_t mSampleRate;
	uint16_t mIsLooped;
	uint16_t mFormat;
	uint32_t mLoopStart;
	uint32_t mLoopEnd;
	uint32_t mCurrentAddress;
	std::array<short, 16> mCoeffs;
	uint16_t mGain;
	uint16_t mPredScale;
	uint16_t mHist1;
	uint16_t mHist2;
	uint16_t mLoopPredScale;
	uint16_t mLoopHist1;
	uint16_t mLoopHist2;
	std::array<short, 11> mPadding;
};

inline void Flip(int16_t &val)
{
	uint16_t uval = static_cast<uint16_t>(val);
	val = (uval << 8) | (uval >> 8);
}

inline void Flip(uint16_t &val)
{
	val = (val << 8) | (val >> 8);
}

inline void Flip(uint32_t &val)
{
	val = (val << 24) | ((val << 8) & 0x00ff0000) |
		((val >> 8) & 0x0000ff00) | (val >> 24);
}

class DspFile
	: public IAudio
{
public:
	DspFile() {};
	DspFile(const std::string &fileName);
	DspFile(const std::string &fileNameL, const std::string &fileNameR);

	virtual ~DspFile() {};

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

	void FlipHeader(DspHeader &header) const;

private:
	DspAdpcmDecoder mDecoder{};
	size_t mOffset = 0;

	const std::string mFormatName = "Nintendo DSP ADPCM";

	int mSampleRate;
	int mNumChannels;

	int mNumSamples;

	bool mIsLooped;
	int mLoopStart;
	int mLoopEnd;

	size_t mBufferSize       = 14 * 72;
	size_t mBufferSizeNibble = 16 * 72;

	bool mIsBufferDone = false;

	std::vector<char> mInBufferL;
	std::vector<char> mInBufferR;

	std::vector<short> mOutBuffer;
};
