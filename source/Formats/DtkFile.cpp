#include "DtkFile.hpp"

DtkFile::DtkFile(const std::string &fileName)
{
	std::ifstream file;

	file.open(fileName, std::fstream::binary);
	file.seekg(0, std::ios::end);

	auto length = static_cast<size_t>(file.tellg());

	mSampleRate  = 48000;
	mNumChannels = 2;
	mNumSamples  = (length - (4 * (length / 32))) * 2;
	mIsLooped    = false;

	mInBuffer.resize(length);
	mOutBuffer.resize(mBufferSize * 2);

	file.seekg(0);
	file.read(mInBuffer.data(), length);
	file.close();
}

const std::string &DtkFile::GetFormatName() const
{
	return mFormatName;
}

const int DtkFile::GetSampleRate() const
{
	return mSampleRate;
}

const int DtkFile::GetNumChannels() const
{
	return mNumChannels;
}

const int DtkFile::GetNumSamples() const
{
	return mNumSamples;
}

const bool DtkFile::GetIsLooped() const
{
	return mIsLooped;
}

const int DtkFile::GetLoopStart() const
{
	return mLoopStart;
}

const int DtkFile::GetLoopEnd() const
{
	return mLoopEnd;
}

const size_t DtkFile::GetBufferSize() const
{
	return mBufferSize;
}

const bool DtkFile::GetIsBufferDone() const
{
	return mIsBufferDone;
}

const std::vector<short> &DtkFile::GetBuffer()
{
	for (int i = 0; i < 64; i++)
	{
		mDecoder.DecodeBlock(mInBuffer, mOutBuffer, mOffset, i * 56);
		mOffset += 32;
	}

	if (mInBuffer.size() == mOffset)
		mIsBufferDone = true;

	return mOutBuffer;
}

void DtkFile::ResetState()
{
	mIsBufferDone = false;
	mOffset = 0;
}