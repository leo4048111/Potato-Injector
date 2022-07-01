#pragma once
#include "utils/utils.hpp"

namespace mem
{
	inline std::vector<std::pair<std::uint32_t, std::wstring>> getProcList() {
		std::vector<std::pair<std::uint32_t, std::wstring>> procList;

		auto hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

		PROCESSENTRY32 e;
		e.dwSize = sizeof(e);

		if (!Process32First(hSnap, &e)) {
			return {};
		}

		while (Process32Next(hSnap, &e)) {
			procList.push_back(std::make_pair(e.th32ProcessID, e.szExeFile));
		}

		CloseHandle(hSnap);
		return procList;
	}

	inline DWORD getProcID(std::wstring_view procname) {
		const auto procList = getProcList();
		if (procList.empty() || procname.empty()) return NULL;

		auto targetName = string::toLower(procname.data());

		for (const auto& proc : procList)
		{
			auto curprocname = string::toLower(proc.second);

			if (curprocname == targetName)
			{
				HANDLE hProc = OpenProcess(PROCESS_VM_READ, false, proc.first);
				if (hProc != nullptr)
				{
					CloseHandle(hProc);
					return proc.first;
				}
			}
		}

		return NULL;
	}

	inline bool openProcess(std::wstring exePath, std::vector<std::wstring> args, PROCESS_INFORMATION& pi) {
		STARTUPINFO si;
		{
			ZeroMemory(&si, sizeof(si));
			si.cb = sizeof(si);
		}

		ZeroMemory(&pi, sizeof(pi));

		std::wstring procCmdLine = exePath;
		for (auto& arg : args) {
			procCmdLine += L" " + arg;
		}

		return CreateProcess(nullptr, procCmdLine.data(), nullptr, nullptr, false, NULL, nullptr,
			nullptr, &si, &pi);
	}
}