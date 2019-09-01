#include "DspFile.hpp"

DspFile::DspFile(const std::string &fileName)
{
	std::ifstream file;
	DspHeader header;

	file.open(fileName, std::fstream::binary);
	file.read(reinterpret_cast<char *>(&header), sizeof(DspHeader));

	file.seekg(0, std::ios::end);

	auto flength = file.tellg();

	file.seekg(0x60);

	if (header.mCurrentAddress > 2)
		FlipHeader(header);

	uint32_t length = ((header.mNibbleCount + 7) & ~7) / 2;

	if (flength > length + 0x100)
	{
		DspHeader headerR;

		mNumChannels = 2;

		mSampleRate = header.mSampleRate;
		mNumSamples = header.mNibbleCount;

		mIsLooped = header.mIsLooped;
		mLoopStart = header.mLoopStart;
		mLoopEnd = header.mLoopEnd;

		mInBufferL.resize(length);
		mInBufferR.resize(length);

		mOutBuffer.resize(mBufferSize * 2);
		file.read(mInBufferL.data(), length);

		file.read(reinterpret_cast<char *>(&headerR), sizeof(DspHeader));

		if (headerR.mCurrentAddress > 2)
			FlipHeader(headerR);

		file.read(mInBufferR.data(), length);

		file.close();

		mDecoder = DspAdpcmDecoder(header.mCoeffs, headerR.mCoeffs);
	}
	else
	{
		mNumChannels = 1;

		mDecoder = DspAdpcmDecoder(header.mCoeffs);

		mSampleRate = header.mSampleRate;
		mNumSamples = header.mNibbleCount;

		mIsLooped  = header.mIsLooped;
		mLoopStart = header.mLoopStart;
		mLoopEnd   = header.mLoopEnd;

		mInBufferL.resize(length);
		mOutBuffer.resize(mBufferSize * 2);
		file.read(mInBufferL.data(), length);
		file.close();
	}
}

DspFile::DspFile(const std::string &fileNameL, const std::string &fileNameR)
{
	std::ifstream fileL, fileR;
	DspHeader headerL, headerR;

	fileL.open(fileNameL, std::fstream::binary);
	fileL.read(reinterpret_cast<char *>(&headerL), sizeof(DspHeader));

	fileR.open(fileNameR, std::fstream::binary);
	fileR.read(reinterpret_cast<char *>(&headerR), sizeof(DspHeader));

	fileL.seekg(0x60);
	fileR.seekg(0x60);

	if (headerL.mCurrentAddress > 2)
	{
		FlipHeader(headerL);
		FlipHeader(headerR);
	}

	uint32_t length = ((headerL.mNibbleCount + 7) & ~7) / 2;

	mNumChannels = 2;

	mSampleRate = headerL.mSampleRate;
	mNumSamples = headerL.mNibbleCount;

	mIsLooped = headerL.mIsLooped;
	mLoopStart = headerL.mLoopStart;
	mLoopEnd = headerL.mLoopEnd;

	mInBufferL.resize(length);
	mInBufferR.resize(length);

	mOutBuffer.resize(mBufferSize * 2);

	fileL.read(mInBufferL.data(), length);
	fileL.close();

	fileR.read(mInBufferR.data(), length);
	fileR.close();

	mDecoder = DspAdpcmDecoder(headerL.mCoeffs, headerR.mCoeffs);
}

const std::string &DspFile::GetFormatName() const
{
	return mFormatName;
}

const int DspFile::GetSampleRate() const
{
	return mSampleRate;
}

const int DspFile::GetNumChannels() const
{
	return mNumChannels;
}

const int DspFile::GetNumSamples() const
{
	return mNumSamples;
}

const bool DspFile::GetIsLooped() const
{
	return mIsLooped;
}

const int DspFile::GetLoopStart() const
{
	return mLoopStart;
}

const int DspFile::GetLoopEnd() const
{
	return mLoopEnd;
}

const size_t DspFile::GetBufferSize() const
{
	return mBufferSize;
}

const bool DspFile::GetIsBufferDone() const
{
	return mIsBufferDone;
}

const std::vector<short> &DspFile::GetBuffer()
{
	if (mInBufferL.size() - mOffset < mBufferSize)
		mIsBufferDone = true;

	if (mNumChannels == 1)
		mDecoder.Decode(mInBufferL, mOutBuffer, mOffset, mBufferSize * 2);
	else
		mDecoder.Decode(mInBufferL, mInBufferR, mOutBuffer, mOffset, mBufferSize * 2);

	mOffset += mBufferSizeNibble / 2;

	return mOutBuffer;
}

void DspFile::ResetState()
{
	mIsBufferDone = false;
	mOffset = 0;
}

void DspFile::FlipHeader(DspHeader &header) const
{
	Flip(header.mSampleCount);
	Flip(header.mNibbleCount);
	Flip(header.mSampleRate);
	Flip(header.mIsLooped);
	Flip(header.mFormat);
	Flip(header.mLoopStart);
	Flip(header.mLoopEnd);
	Flip(header.mCurrentAddress);
	Flip(header.mGain);
	Flip(header.mPredScale);
	Flip(header.mHist1);
	Flip(header.mHist2);
	Flip(header.mLoopPredScale);
	Flip(header.mLoopHist1);
	Flip(header.mLoopHist2);

	for (int i = 0; i < 16; i++)
		Flip(header.mCoeffs[i]);
}
