#pragma once

#include <cstdint>
#include <filesystem>
#include <fmt/core.h>

#ifdef _WIN32
#include <Windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

namespace Helpers {
	std::filesystem::path GetExecutableDirectory();
}
