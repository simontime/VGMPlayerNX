#include "DspAdpcm.hpp"
#include "../Globals.hpp"

DspAdpcmDecoder::DspAdpcmDecoder(const std::array<short, 16> &coeffs)
{
	chL.mCoeffs = coeffs;
}

DspAdpcmDecoder::DspAdpcmDecoder(const std::array<short, 16> &coeffsL, const std::array<short, 16> &coeffsR)
{
	chL.mCoeffs = coeffsL;
	chR.mCoeffs = coeffsR;
}

inline int divRoundUp(int dividend, int divisor)
{
	return (dividend + divisor - 1) / divisor;
}

inline int min(int a, int b)
{
	return a < b ? a : b;
}

// Reimplementation based on reverse engineering Super Mario Odyssey (nn::atk::detail::DecodeDspAdpcm).

void DspAdpcmDecoder::Decode(const std::vector<char> &src, std::vector<short> &dst, int startAt, int numSamples)
{
	int dstPtr = 0;

	for (int i = 0; i < divRoundUp(numSamples, 14); i++)
	{
		const int numRead = min(14, numSamples);

		const int pred  = src[startAt] >> 4;
		const int scale = 1 << (src[startAt++] & 0xf);

		const int c1 = chL.mCoeffs[pred * 2 + 0];
		const int c2 = chL.mCoeffs[pred * 2 + 1];

		for (int j = 0; j < numRead; j++)
		{
			short nibble = j & 1 ? src[startAt++] & 0xf : src[startAt] >> 4;

			nibble <<= 12; nibble >>= 1;

			int samp = scale * nibble + c1 * chL.mHist1 + c2 * chL.mHist2;
			samp >>= 10; ++samp >>= 1;

			chL.mHist2 = chL.mHist1;
			dst[dstPtr++] = chL.mHist1 = std::clamp<int>(samp, INT16_MIN, INT16_MAX);
		}

		numSamples -= numRead;
	}
}

void DspAdpcmDecoder::Decode(const std::vector<char> &srcL, const std::vector<char> &srcR, std::vector<short> &dst, const int startAt, int numSamples)
{
	int srcLPtr = startAt;
	int srcRPtr = startAt;

	int dstPtr = 0;

	for (int i = 0; i < divRoundUp(numSamples, 14); i++)
	{
		const int numRead = min(14, numSamples);

		/* Left channel */
		const int predL  = srcL[srcLPtr] >> 4;
		const int scaleL = 1 << (srcL[srcLPtr++] & 0xf);
		
		const int c1L = chL.mCoeffs[predL * 2 + 0];
		const int c2L = chL.mCoeffs[predL * 2 + 1];

		/* Right channel */
		const int predR  = srcR[srcRPtr] >> 4;
		const int scaleR = 1 << (srcR[srcRPtr++] & 0xf);

		const int c1R = chR.mCoeffs[predR * 2 + 0];
		const int c2R = chR.mCoeffs[predR * 2 + 1];

		for (int j = 0; j < numRead; j++)
		{
			/* Left channel */
			short nibbleL = j & 1 ? srcL[srcLPtr++] & 0xf : srcL[srcLPtr] >> 4;

			nibbleL <<= 12; nibbleL >>= 1;

			int sampL = scaleL * nibbleL + c1L * chL.mHist1 + c2L * chL.mHist2;
			sampL >>= 10; ++sampL >>= 1;

			chL.mHist2 = chL.mHist1;
			dst[dstPtr++] = chL.mHist1 = static_cast<short>(std::clamp<int>(sampL, INT16_MIN, INT16_MAX));

			/* Right channel */
			short nibbleR = j & 1 ? srcR[srcRPtr++] & 0xf : srcR[srcRPtr] >> 4;

			nibbleR <<= 12; nibbleR >>= 1;

			int sampR = scaleR * nibbleR + c1R * chR.mHist1 + c2R * chR.mHist2;
			sampR >>= 10; ++sampR >>= 1;

			chR.mHist2 = chR.mHist1;
			dst[dstPtr++] = chR.mHist1 = static_cast<short>(std::clamp<int>(sampR, INT16_MIN, INT16_MAX));
		}

		numSamples -= numRead;
	}
}
