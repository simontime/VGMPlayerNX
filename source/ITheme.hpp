#pragma once

#include <string>
#include <vector>

class ITheme
{
public:
	virtual ~ITheme() {};

	virtual const std::string       &GetThemeName()  const = 0;

	virtual const std::vector<char> &GetAlbumArt()   const = 0;
	virtual const std::vector<char> &GetBackground() const = 0;
	virtual const std::vector<char> &GetFont()       const = 0;
};
