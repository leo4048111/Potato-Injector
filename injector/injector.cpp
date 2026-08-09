#include "pch.h"
#include "injector.hpp"

Injector* Injector::m_inst = nullptr;

void Injector::initialize()
{
	return;
}

bool Injector::inject(std::string dllPath)
{
	g_menu->isInjecting = true;
	std::vector<BYTE> buffer;
	if (!utils::readFileToMem(std::filesystem::absolute(dllPath), buffer))
	{
		g_menu->isInjecting = false;
		return false;
	}

	if(this->isCustomProcess) {
		if (!this->map(customProcessName, customProcessName, buffer))
		{
			g_menu->isInjecting = false;
			return false;
		}
	}
	else {
		if (!this->map(vars::str_game_process_name.data(), vars::str_game_mod_name.data(), buffer))
		{
			g_menu->isInjecting = false;
			return false;
		}
	}

	g_menu->isInjecting = false;
	if (this->shouldAutoExit) g_menu->isMenuOn = false;
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
		auto mods = proc.modules().GetAllModules();

		auto toLower = [](const std::wstring& str) {
			std::wstring lowerStr = str;
			std::transform(lowerStr.begin(), lowerStr.end(), lowerStr.begin(),
				[](wchar_t c) { return std::towlower(c); });
			return lowerStr;
			};

		for (const auto& mod : mods) {
			if (toLower(mod.first.first) == toLower(modname.data()))
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

