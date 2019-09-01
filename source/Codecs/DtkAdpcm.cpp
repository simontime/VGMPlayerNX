#include "DtkAdpcm.hpp"
#include "../Globals.hpp"

// Reimplementation based on reverse engineering dtkmake/trkmake v1.4.

const short DtkAdpcmDecoder::DecodeSample(const short inSample, const short predScale, const bool rightChannel)
{
	int &hist1 = rightChannel ? chR.mHist1 : chL.mHist1;
	int &hist2 = rightChannel ? chR.mHist2 : chL.mHist2;

	const auto mul = [&]() {
		switch (predScale >> 4)
		{
			case 1: return hist1 * 0x3c;
			case 2: return hist1 * 0x73 - hist2 * 0x34;
			case 3: return hist1 * 0x62 - hist2 * 0x37;
			default: return 0;
		}
	};
	
	int samp = (static_cast<short>(inSample << 12) >> (predScale & 0xf)) << 6;
	samp += std::clamp<int>((mul() + 32) >> 6, DTK_MIN, DTK_MAX);

	hist2 = hist1;
	hist1 = samp;

	return static_cast<short>(std::clamp<int>(samp >> 6, INT16_MIN, INT16_MAX));
}

void DtkAdpcmDecoder::DecodeBlock(const std::vector<char> &src, std::vector<short> &dst, const int inStartAt, const int outStartAt)
{
	for (int i = 0; i < 28; i++)
	{
		dst[outStartAt + 2 * i + 0] = DecodeSample(src[inStartAt + i + 4] & 0xF, src[inStartAt + 0], false);
		dst[outStartAt + 2 * i + 1] = DecodeSample(src[inStartAt + i + 4] >> 4,  src[inStartAt + 1], true);
	}
}
