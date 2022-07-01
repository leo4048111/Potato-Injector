#pragma once
#include <string>

namespace vars
{
	inline std::wstring_view str_steam_process_name{ L"steam.exe" };
	inline std::wstring_view str_game_process_name{ L"csgo.exe" };
	inline std::wstring_view str_dll_name{ L"cheat.dll" };
	inline std::wstring_view str_steam_mod_name{ L"tier0_s.dll" };
	inline std::wstring_view str_game_mod_name{ L"serverbrowser.dll" };
	inline uint32_t game_appid{ 730 };
	inline std::wstring_view str_game_launch_opts{ L"-console -worldwide -insecure -novid" };
	inline std::wstring_view str_dll_dir_path{ L"./dlls" };
}