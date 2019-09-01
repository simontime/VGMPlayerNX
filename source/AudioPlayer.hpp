#pragma once

#include "IAudio.hpp"
#include "Globals.hpp"
#include "Graphics.hpp"
#include "Utils.hpp"

constexpr const char *dspStereoSuffixes[] =
{
	"_l", "_L", ".l", ".L", "_0", ".0",
	"_r", "_R", ".r", ".R", "_1", ".1",
};

namespace Audio
{
	PlayStatus Play(const std::filesystem::path &path, const bool loop);
}