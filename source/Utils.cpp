#include "Utils.hpp"

namespace Utils
{
	namespace File
	{
		bool Exists(const std::string &fileName)
		{
			return std::filesystem::exists(fileName);
		}

		std::vector<std::filesystem::path> GetAllInDirectory(const std::string &dirName)
		{
			std::vector<std::filesystem::path> dirList;

			for (const auto &dirent : std::filesystem::directory_iterator(dirName))
				dirList.push_back(dirent);

			return dirList;
		}
	}
}