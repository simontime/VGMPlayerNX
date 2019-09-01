#include "GMEHandler.hpp"

GMEHandler::GMEHandler(const std::string &fileName)
{
	gme_equalizer_t eq = { 5.0, 15 };

	gme_type_t type;

	gme_identify_file(fileName.c_str(), &type);

	gme_open_file(fileName.c_str(), &emu, 48000);
	gme_enable_accuracy(emu, true);
	gme_set_equalizer(emu, &eq);
	gme_start_track(emu, 0); // TO-DO: Add proper interface for selecting a sub-track.

	mFormatName = type->system;

	mSampleRate = 48000;
	mNumChannels = 2;

	mOutBuffer.resize(mBufferSize * 2);
}

GMEHandler::~GMEHandler()
{
	gme_delete(emu);
}

const std::string &GMEHandler::GetFormatName() const
{
	return mFormatName;
}

const int GMEHandler::GetSampleRate() const
{
	return mSampleRate;
}

const int GMEHandler::GetNumChannels() const
{
	return mNumChannels;
}

const int GMEHandler::GetNumSamples() const
{
	return mNumSamples;
}

const bool GMEHandler::GetIsLooped() const
{
	return mIsLooped;
}

const int GMEHandler::GetLoopStart() const
{
	return mLoopStart;
}

const int GMEHandler::GetLoopEnd() const
{
	return mLoopEnd;
}

const size_t GMEHandler::GetBufferSize() const
{
	return mBufferSize;
}

const bool GMEHandler::GetIsBufferDone() const
{
	return mIsBufferDone;
}

const std::vector<short> &GMEHandler::GetBuffer()
{
	gme_play(emu, mBufferSize * 2, mOutBuffer.data());

	if (gme_track_ended(emu))
		mIsBufferDone = true;

	return mOutBuffer;
}

void GMEHandler::ResetState()
{
}