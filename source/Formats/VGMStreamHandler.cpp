#include "VGMStreamHandler.hpp"

VGMStreamHandler::VGMStreamHandler(const std::string &fileName)
{
	vgm = init_vgmstream(fileName.c_str());

	mFormatName = get_vgmstream_coding_description(vgm->coding_type);

	mSampleRate = vgm->sample_rate;
	mNumChannels = vgm->channels;

	mOutBuffer.resize(mBufferSize * 2);
}

VGMStreamHandler::~VGMStreamHandler()
{
	close_vgmstream(vgm);
}

const std::string &VGMStreamHandler::GetFormatName() const
{
	return mFormatName;
}

const int VGMStreamHandler::GetSampleRate() const
{
	return mSampleRate;
}

const int VGMStreamHandler::GetNumChannels() const
{
	return mNumChannels;
}

const int VGMStreamHandler::GetNumSamples() const
{
	return mNumSamples;
}

const bool VGMStreamHandler::GetIsLooped() const
{
	return mIsLooped;
}

const int VGMStreamHandler::GetLoopStart() const
{
	return mLoopStart;
}

const int VGMStreamHandler::GetLoopEnd() const
{
	return mLoopEnd;
}

const size_t VGMStreamHandler::GetBufferSize() const
{
	return mBufferSize;
}

const bool VGMStreamHandler::GetIsBufferDone() const
{
	return mIsBufferDone;
}

const std::vector<short> &VGMStreamHandler::GetBuffer()
{
	render_vgmstream(mOutBuffer.data(), mBufferSize, vgm);

	if (vgm->current_sample >= vgm->num_samples)
		mIsBufferDone = true;

	return mOutBuffer;
}

void VGMStreamHandler::ResetState()
{
	reset_vgmstream(vgm);
}