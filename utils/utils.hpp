#pragma once
#include "winreg/winreg.hpp"

namespace string
{
	inline std::wstring toLower(std::wstring s) {
		std::transform(s.begin(), s.begin(), s.end(), static_cast<int(*)(int)>(&std::tolower));
		return s;
	}

	template<typename ... arg>
	static std::wstring format(std::wstring_view  fmt, arg ... args) {
		const int size = std::swprintf(nullptr, NULL, fmt.data(), args ...) + 1;
		const auto buf = std::make_unique<wchar_t[]>(size);
		std::swprintf(buf.get(), size, fmt.data(), args ...);

		return std::wstring(buf.get(), buf.get() + size - 1);
	}
}

namespace utils
{
	inline std::wstring getSteamPath() {
		winreg::RegKey key{ HKEY_CURRENT_USER, L"SOFTWARE\\Valve\\Steam" };
		auto path = key.GetStringValue(L"SteamExe");
		return path;
	}

	inline bool readFileToMem(const std::filesystem::path& path, std::vector<BYTE>& buffer) {
		std::ifstream file(path, std::ios::binary);
		if (file.fail()) return false;

		buffer.assign((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

		file.close();

		return true;
	}
}