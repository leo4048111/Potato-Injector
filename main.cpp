#include "pch.h"

namespace
{
	constexpr auto randomizedInstanceArgument = "--randomized-instance";
	constexpr auto randomizedInstanceArgumentWide = L"--randomized-instance";

	std::wstring getCurrentExecutablePath()
	{
		std::wstring path(MAX_PATH, L'\0');

		for (;;)
		{
			const DWORD length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
			if (length == 0)
				return {};

			if (length < path.size() - 1)
			{
				path.resize(length);
				return path;
			}

			path.resize(path.size() * 2);
		}
	}

	std::wstring makeRandomExecutableName()
	{
		static constexpr wchar_t alphabet[] = L"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
		std::random_device randomDevice;
		std::mt19937_64 generator(randomDevice());
		std::uniform_int_distribution<size_t> distribution(0, std::size(alphabet) - 2);

		std::wstring name;
		name.reserve(20);
		for (size_t i = 0; i < 16; ++i)
			name.push_back(alphabet[distribution(generator)]);

		name += L".exe";
		return name;
	}

	bool launchRandomizedInstance()
	{
		const auto currentExecutable = getCurrentExecutablePath();
		if (currentExecutable.empty())
			return false;

		std::error_code error;
		const auto instanceDirectory = std::filesystem::temp_directory_path(error) / L"PotatoInjectorInstances";
		if (error)
			return false;

		std::filesystem::create_directories(instanceDirectory, error);
		if (error)
			return false;

		// Remove copies left by completed runs. An active executable remains locked and is skipped.
		for (std::filesystem::directory_iterator iterator(instanceDirectory, error), end; !error && iterator != end; iterator.increment(error))
		{
			if (iterator->is_regular_file(error) && iterator->path().extension() == L".exe")
			{
				std::error_code removeError;
				std::filesystem::remove(iterator->path(), removeError);
			}
		}

		std::filesystem::path randomizedExecutable;
		for (int attempt = 0; attempt < 8; ++attempt)
		{
			randomizedExecutable = instanceDirectory / makeRandomExecutableName();
			if (CopyFileW(currentExecutable.c_str(), randomizedExecutable.c_str(), TRUE))
				break;

			randomizedExecutable.clear();
		}

		if (randomizedExecutable.empty())
			return false;

		std::wstring commandLine = L"\"" + randomizedExecutable.wstring() + L"\" " + randomizedInstanceArgumentWide;
		STARTUPINFOW startupInfo{ sizeof(startupInfo) };
		PROCESS_INFORMATION processInfo{};

		const BOOL created = CreateProcessW(
			randomizedExecutable.c_str(),
			commandLine.data(),
			nullptr,
			nullptr,
			FALSE,
			0,
			nullptr,
			nullptr,
			&startupInfo,
			&processInfo);

		if (!created)
		{
			std::filesystem::remove(randomizedExecutable, error);
			return false;
		}

		CloseHandle(processInfo.hThread);
		CloseHandle(processInfo.hProcess);
		return true;
	}
}

int main(int argc, char* argv[])
{
	const bool isRandomizedInstance = argc > 1 && std::strcmp(argv[1], randomizedInstanceArgument) == 0;
	if (!isRandomizedInstance)
	{
		if (launchRandomizedInstance())
			return 0;

		MessageBoxW(nullptr, L"Failed to start the randomized application instance.", L"Startup error", MB_OK | MB_ICONERROR);
		return 1;
	}

	FreeConsole();
	g_menu->initialize();
	g_menu->loop();
	return 0;
}
