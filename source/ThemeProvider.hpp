#pragma once

#include <memory>

#include "ITheme.hpp"
#include "ThemePak.hpp"

class ThemeProvider
	: public ITheme, private ThemePak
{
public:
	ThemeProvider() {};
	ThemeProvider(const std::string &fileName);

	virtual ~ThemeProvider() {};

	const std::string       &GetThemeName()  const;

	const std::vector<char> &GetAlbumArt()   const;
	const std::vector<char> &GetBackground() const;
	const std::vector<char> &GetFont()       const;

private:
	std::string mThemeName = "Default";

	std::vector<char> mAlbumArt;
	std::vector<char> mBackground;
	std::vector<char> mFont;
};