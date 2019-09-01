#include "ThemeProvider.hpp"

ThemeProvider::ThemeProvider(const std::string &fileName) 
	: ThemePak(fileName)
{
	mAlbumArt   = GetFile("album_art.bmp");
	mBackground = GetFile("background.bmp");
	mFont       = GetFile("font.ttf");
	mHandle.close();
}

const std::string &ThemeProvider::GetThemeName() const
{
	return mThemeName;
}

const std::vector<char> &ThemeProvider::GetAlbumArt() const
{
	return mAlbumArt;
}

const std::vector<char> &ThemeProvider::GetBackground() const
{
	return mBackground;
}

const std::vector<char> &ThemeProvider::GetFont() const
{
	return mFont;
}
