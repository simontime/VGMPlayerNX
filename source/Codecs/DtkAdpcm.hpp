#pragma once

#include <algorithm>
#include <vector>

class DtkAdpcmDecoder
{
public:
	DtkAdpcmDecoder() {};

	void DecodeBlock(const std::vector<char> &src, std::vector<short> &dst, const int inStartAt, const int outStartAt);

private:
	static constexpr int DTK_MIN = -0x200000;
	static constexpr int DTK_MAX = 0x1fffff;

	struct Context
	{
		int mHist1 = 0;
		int mHist2 = 0;
	};

	Context chL;
	Context chR;

	const short DecodeSample(const short inSample, const short predScale, const bool rightChannel);
};