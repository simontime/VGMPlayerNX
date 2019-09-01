#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace Utils
{
	namespace File
	{
		bool Exists(const std::string &fileName);
		std::vector<std::filesystem::path> GetAllInDirectory(const std::string &dirName);
	}
}