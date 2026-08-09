#pragma once

#include <BlackBone/Process/Process.h>

class Injector
{
public:
	Injector() = default;
	~Injector() = default;

	void initialize();

	bool inject(std::string dllPath);

	bool steamRunning{ false };
	bool csgoRunning{ false };

	bool shouldAutoExit{ false };
	bool isCustomProcess{ false };

	::std::wstring customProcessName{ L"godmode.exe" };

private:
	static Injector* m_inst;

	bool map(std::wstring_view procname, std::wstring_view modname, const std::vector<BYTE>& buffer, blackbone::eLoadFlags flags = blackbone::NoFlags);

};

inline auto g_injector = std::make_unique<Injector>();
