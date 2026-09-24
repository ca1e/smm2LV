#include "Helpers.hpp"

std::filesystem::path Helpers::GetExecutableDirectory() {
#ifdef _WIN32
	wchar_t path[MAX_PATH] = { 0 };
	GetModuleFileNameW(NULL, path, MAX_PATH);
	return path;
#elif defined(__APPLE__)
	char buf [PATH_MAX];
	uint32_t bufsize = PATH_MAX;
	if(_NSGetExecutablePath(buf, &bufsize)) {
		return std::string();
	} else {
		return std::string(buf);
	}
#else
	char result[PATH_MAX];
	ssize_t count = readlink("/proc/self/exe", result, PATH_MAX);
	return std::string(result, (count > 0) ? count : 0);
#endif
}
