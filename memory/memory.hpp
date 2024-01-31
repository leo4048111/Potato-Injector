#pragma once
#include "utils/utils.hpp"

namespace mem
{
	struct CompareProc {
		bool operator()(const std::pair<std::uint32_t, std::wstring>& lhs, const std::pair<std::uint32_t, std::wstring>& rhs) const {
			return lhs.second < rhs.second;
		}
	};

	inline bool isSystemProcess(const std::wstring& name) {
		static const std::set<std::wstring> systemProcesses = {
			L"System", L"svchost.exe", L"csrss.exe", L"smss.exe", L"wininit.exe", L"services.exe"
		};
		return systemProcesses.find(name) != systemProcesses.end();
	}

	inline std::set<std::pair<std::uint32_t, std::wstring>, CompareProc> getProcList() {
		std::set<std::pair<std::uint32_t, std::wstring>, CompareProc> procList;

		auto hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, NULL);

		PROCESSENTRY32 e;
		e.dwSize = sizeof(e);

		if (!Process32First(hSnap, &e)) {
			CloseHandle(hSnap);
			return {};
		}

		do {
			if (!isSystemProcess(e.szExeFile)) {
				procList.insert(std::make_pair(e.th32ProcessID, e.szExeFile));
			}
		} while (Process32Next(hSnap, &e));

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