#pragma once

#include <BlackBone/Process/Process.h>

class Injector
{
public:
	Injector() = default;
	~Injector() = default;

	void initialize();

	bool bypassVAC();

	bool inject(std::wstring dllPath);

	bool vacBypassed{ false };
	bool steamRunning{ false };
	bool csgoRunning{ false };

	bool shouldAutoExit{ false };
	bool shouldAutoStart{ false };

private:
	static Injector* m_inst;

	bool map(std::wstring_view procname, std::wstring_view modname, std::vector<BYTE> buffer, blackbone::eLoadFlags flags = blackbone::WipeHeader);

	void closeProcesses(std::vector<std::wstring> processes);
};

inline auto g_injector = std::make_unique<Injector>();