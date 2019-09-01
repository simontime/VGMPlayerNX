#pragma once

#include <algorithm>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "tinf.h"

class ThemePak
{
public:
	struct HeaderLengths
	{
		uint16_t mCompressedLength;
		uint16_t mDecompressedLength;
	};

	struct FileLengths
	{
		uint32_t mCompressedLength;
		uint32_t mDecompressedLength;
	};

	struct FileEntry
	{
		std::string mName;
		uint32_t mOffset;
		FileLengths mLengths;
	};

	ThemePak(){};
	ThemePak(const std::string &fileName);
	const std::vector<char> GetFile(const std::string &name);

	std::vector<FileEntry> mFiles;

protected:
	std::ifstream mHandle;
};