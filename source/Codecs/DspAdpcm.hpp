#pragma once

#include <algorithm>
#include <array>
#include <vector>

class DspAdpcmDecoder
{
public:
	DspAdpcmDecoder() {};
	DspAdpcmDecoder(const std::array<short, 16> &coeffs);
	DspAdpcmDecoder(const std::array<short, 16> &coeffsL, const std::array<short, 16> &coeffsR);

	void Decode(const std::vector<char> &src, std::vector<short> &dst, int startAt, int numSamples);
	void Decode(const std::vector<char> &srcL, const std::vector<char> &srcR, std::vector<short> &dst, const int startAt, int numSamples);

private:
	struct Context
	{
		std::array<short, 16> mCoeffs;
		short mHist1 = 0;
		short mHist2 = 0;
	};

	Context chL;
	Context chR;
};