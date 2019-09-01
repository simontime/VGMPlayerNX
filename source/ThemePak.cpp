#include "ThemePak.hpp"

ThemePak::ThemePak(const std::string &fileName)
{
	HeaderLengths hLen;

	mHandle.open(fileName, std::fstream::binary); // Open file in binary mode.
	mHandle.read(reinterpret_cast<char *>(&hLen), sizeof(HeaderLengths)); // Read HeaderLengths struct.

	// Allocate vector sizes according to header's compressed and decompressed lengths.
	std::vector<char> inBuf(hLen.mCompressedLength); 
	std::vector<char> outBuf(hLen.mDecompressedLength);

	mHandle.read(inBuf.data(), hLen.mCompressedLength); // Read string section into vector buffer.

	// Decompress string section into new vector buffer.
	tinf_uncompress(outBuf.data(), reinterpret_cast<uint32_t *>(&hLen), // (Ab)using HeaderLengths struct.
		inBuf.data(), hLen.mCompressedLength);

	// Resize vector fields to accommodate for number of files in pak, calculated by counting string null-terminators.
	mFiles.resize(std::count(outBuf.begin(), outBuf.end(), 0));

	char *strBuf = outBuf.data(); // Get char * from vector.

	for (auto &file : mFiles) // Fill fields with values for all files.
	{
		mHandle.read(reinterpret_cast<char *>(&file.mLengths), sizeof(FileLengths)); // Read FileLengths struct into mLengths.

		file.mOffset = static_cast<uint32_t>(mHandle.tellg()); // Set mOffset to current position within pak.
		file.mName   = strBuf; // Set mName to null-terminated string from buffer.

		mHandle.seekg(file.mLengths.mCompressedLength, std::ios_base::cur); // Seek to next file within pak.
		strBuf += file.mName.length() + 1; // Skip to next string within buffer, passing previous string's null-terminator.
	}
}

// Returns file within pak as vector containing its data.
const std::vector<char> ThemePak::GetFile(const std::string &name)
{
	// Look for supplied file name within pak entries.
	const auto iter = std::find_if(mFiles.begin(), mFiles.end(), 
	[&] (const FileEntry &entry) {
		return !entry.mName.compare(name);
	});

	// If found, calculate distance between beginning and iterator, returning its entry index.
	const auto found = std::distance(mFiles.begin(), iter);

	if (iter == mFiles.end()) // If iterator's position = the end of mNames, file not found within pak.
		return {}; // Return empty vector.
	else
	{
		// Allocate vector sizes according to file's compressed and decompressed lengths.
		std::vector<char> outBuf(mFiles.at(found).mLengths.mDecompressedLength);
		std::vector<char> cmpBuf(mFiles.at(found).mLengths.mCompressedLength);

		mHandle.seekg(mFiles.at(found).mOffset); // Seek to offset of file within pak.
		mHandle.read(cmpBuf.data(), mFiles.at(found).mLengths.mCompressedLength); // Read compressed file into vector buffer.

		tinf_uncompress(outBuf.data(), &mFiles.at(found).mLengths.mDecompressedLength, // Decompress file into new vector buffer.
			cmpBuf.data(), mFiles.at(found).mLengths.mCompressedLength);

		return outBuf; // Return vector containing decompressed data.
	}
}
