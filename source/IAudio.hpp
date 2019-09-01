#pragma once

#include <string>
#include <vector>

class IAudio
{
public:
	virtual ~IAudio() {};

	virtual const std::string &GetFormatName() const = 0;

	virtual const int GetSampleRate()  const = 0;
	virtual const int GetNumChannels() const = 0;

	virtual const int GetNumSamples() const = 0;

	virtual const bool GetIsLooped()  const = 0;
	virtual const int  GetLoopStart() const = 0;
	virtual const int  GetLoopEnd()   const = 0;

	virtual const size_t GetBufferSize()   const = 0;
	virtual const bool   GetIsBufferDone() const = 0;

	virtual const std::vector<short> &GetBuffer() = 0;

	virtual void ResetState() = 0;
};
