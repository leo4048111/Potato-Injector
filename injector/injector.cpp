#include "pch.h"
#include "injector.hpp"
#include "vac3_bypass/vac3_bypass.hpp"

Injector* Injector::m_inst = nullptr;

void Injector::initialize()
{
	return;
}

bool Injector::bypassVAC()
{
	g_menu->isPatchingVac = true;
	this->closeProcesses({ vars::str_game_process_name.data(), vars::str_steam_process_name.data()});
	
	const auto& steamPath = utils::getSteamPath();
	if (steamPath.empty())
	{
		g_menu->isPatchingVac = false;
		return false;
	}
	std::wstring launchGame = string::format(L"-applaunch %d", vars::game_appid);
	PROCESS_INFORMATION pi = {};
#ifdef _DEBUG
	bool result = mem::openProcess(steamPath, { launchGame, L" -windowed -w 1280 -h 720" ,vars::str_game_launch_opts.data() }, pi);
#else
	bool result = mem::openProcess(steamPath, { launchGame, vars::str_game_launch_opts.data() }, pi);
#endif
	if (!result)
	{
		g_menu->isPatchingVac = false;
		return false;
	}

	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	
	std::vector<BYTE> vacBypassInstBuf(std::begin(vac3_data), std::end(vac3_data));

	if (!this->map(vars::str_steam_process_name.data(), vars::str_steam_mod_name.data(), vacBypassInstBuf))
	{
		g_menu->isPatchingVac = false;
		return false;
	}

	g_menu->isPatchingVac = false;
	this->vacBypassed = true;
	return true;

}

bool Injector::inject(std::wstring dllPath)
{
	std::vector<BYTE> buffer;
	if(!utils::readFileToMem(std::filesystem::absolute(dllPath), buffer))
		return false;

	if (!this->map(vars::str_game_process_name.data(), vars::str_game_mod_name.data(), buffer))
		return false;

	return true;
}

bool Injector::map(std::wstring_view procname, std::wstring_view modname, std::vector<BYTE> buffer, blackbone::eLoadFlags flags)
{
	bool mappingFinished = false;
	DWORD pID = NULL;
	do {
		pID = mem::getProcID(procname);
		std::this_thread::sleep_for(500ms);
	} while (!pID);

	blackbone::Process proc;
	proc.Attach(pID, PROCESS_ALL_ACCESS);
	std::thread([&] {
		do {
			if (mem::getProcID(procname) == NULL)
			{
				mappingFinished = true;         //When process exits before mod is ready, this will make sure mapping function aborts.
				break;
			}
			std::this_thread::sleep_for(500ms);
		} while (!mappingFinished);
		}).detach();

	bool modReady = false;
	while (!modReady) {
		if (mappingFinished)
		{
			proc.Detach();
			return false;
		}
		for (const auto& mod : proc.modules().GetAllModules()) {
			if (mod.first.first == modname)
			{
				modReady = true;
				break;
			}
		}
		if (modReady) break;

		std::this_thread::sleep_for(1s);
	}

	const auto modCallback = [](blackbone::CallbackType type, void* context, blackbone::Process& process, const blackbone::ModuleData& modInfo)
	{
		if (type == blackbone::PreCallback)
		{
			if (modInfo.name == L"user32.dll")
				return blackbone::LoadData(blackbone::MT_Native, blackbone::Ldr_Ignore);
		}

		return blackbone::LoadData(blackbone::MT_Default, blackbone::Ldr_Ignore);
	};

	const auto result = proc.mmap().MapImage(buffer.size(), buffer.data(), false, flags, modCallback);
	if (!result.success())
	{
		proc.Detach();
		return false;
	}

	proc.Detach();
	mappingFinished = true;
	std::this_thread::sleep_for(1s);   //wait for its child thread to exit.
	return true;
}

void Injector::closeProcesses(std::vector<std::wstring> processes)
{
	for (const auto& proc : processes)
	{
		DWORD pID = mem::getProcID(proc);
		if (pID)
		{
			HANDLE hProc = OpenProcess(PROCESS_TERMINATE, false, pID);
			if (hProc != nullptr)
			{
				TerminateProcess(hProc, 9);
				CloseHandle(hProc);
			}
		}
	}
}
