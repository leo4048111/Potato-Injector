#include "pch.h"
#include "injector.hpp"

namespace
{
	constexpr auto kProcessWaitTimeout = 30s;
	constexpr auto kModuleWaitTimeout = 20s;
	constexpr auto kPollInterval = 250ms;
	// BlackBone's predefined mapping access set includes the rights required by
	// its remote execution and handle-management paths without using ALL_ACCESS.
	constexpr DWORD kMapProcessAccess = DEFAULT_ACCESS_P;

	bool rangeWithin(std::size_t offset, std::size_t length, std::size_t total)
	{
		return offset <= total && length <= total - offset;
	}

	bool rangeWithinRva(DWORD rva, DWORD size, const IMAGE_NT_HEADERS64& nt, const IMAGE_SECTION_HEADER* sections, std::size_t fileSize)
	{
		if (size == 0)
			return true;

		const auto imageEnd = static_cast<std::uint64_t>(rva) + size;
		if (imageEnd > nt.OptionalHeader.SizeOfImage)
			return false;

		if (rva < nt.OptionalHeader.SizeOfHeaders)
			return rangeWithin(rva, size, fileSize);

		for (WORD index = 0; index < nt.FileHeader.NumberOfSections; ++index)
		{
			const auto& section = sections[index];
			const auto sectionStart = static_cast<std::uint64_t>(section.VirtualAddress);
			const auto sectionSpan = std::max(section.Misc.VirtualSize, section.SizeOfRawData);
			const auto sectionEnd = sectionStart + sectionSpan;

			if (rva < sectionStart || imageEnd > sectionEnd || section.SizeOfRawData == 0)
				continue;

			const auto rawOffset = static_cast<std::uint64_t>(section.PointerToRawData) + (rva - section.VirtualAddress);
			return rawOffset <= fileSize && size <= fileSize - rawOffset;
		}

		return false;
	}

	bool validatePeImage(const std::vector<BYTE>& buffer, std::wstring& reason, bool& hasRelocations)
	{
		hasRelocations = false;
		if (buffer.size() < sizeof(IMAGE_DOS_HEADER))
		{
			reason = L"buffer is smaller than IMAGE_DOS_HEADER";
			return false;
		}

		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(buffer.data());
		if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0)
		{
			reason = L"invalid DOS header";
			return false;
		}

		const auto ntOffset = static_cast<std::size_t>(dos->e_lfanew);
		if (!rangeWithin(ntOffset, sizeof(IMAGE_NT_HEADERS64), buffer.size()))
		{
			reason = L"NT header is outside the file";
			return false;
		}

		const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(buffer.data() + ntOffset);
		if (nt->Signature != IMAGE_NT_SIGNATURE || nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64)
		{
			reason = L"image is not a valid x64 PE";
			return false;
		}

		if (nt->FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER64) ||
			nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
		{
			reason = L"invalid x64 optional header";
			return false;
		}

		if (nt->FileHeader.NumberOfSections == 0 || nt->FileHeader.NumberOfSections > 96)
		{
			reason = L"invalid section count";
			return false;
		}

		if (nt->OptionalHeader.SizeOfImage == 0 ||
			nt->OptionalHeader.SizeOfHeaders > nt->OptionalHeader.SizeOfImage ||
			nt->OptionalHeader.SizeOfHeaders > buffer.size())
		{
			reason = L"invalid image/header size";
			return false;
		}

		const auto sectionTableOffset = ntOffset + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + nt->FileHeader.SizeOfOptionalHeader;
		const auto sectionTableSize = static_cast<std::size_t>(nt->FileHeader.NumberOfSections) * sizeof(IMAGE_SECTION_HEADER);
		if (!rangeWithin(sectionTableOffset, sectionTableSize, buffer.size()))
		{
			reason = L"section table is outside the file";
			return false;
		}

		const auto* sections = reinterpret_cast<const IMAGE_SECTION_HEADER*>(buffer.data() + sectionTableOffset);
		for (WORD index = 0; index < nt->FileHeader.NumberOfSections; ++index)
		{
			const auto& section = sections[index];
			if (section.SizeOfRawData != 0 && !rangeWithin(section.PointerToRawData, section.SizeOfRawData, buffer.size()))
			{
				reason = L"section raw data is outside the file";
				return false;
			}

			const auto sectionEnd = static_cast<std::uint64_t>(section.VirtualAddress) +
				std::max(section.Misc.VirtualSize, section.SizeOfRawData);
			if (sectionEnd > nt->OptionalHeader.SizeOfImage)
			{
				reason = L"section virtual range exceeds SizeOfImage";
				return false;
			}
		}

		if (nt->OptionalHeader.AddressOfEntryPoint != 0 &&
			nt->OptionalHeader.AddressOfEntryPoint >= nt->OptionalHeader.SizeOfImage)
		{
			reason = L"entry point is outside the image";
			return false;
		}

		const auto& directories = nt->OptionalHeader.DataDirectory;
		if (nt->OptionalHeader.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_IMPORT)
		{
			const auto& imports = directories[IMAGE_DIRECTORY_ENTRY_IMPORT];
			if (!rangeWithinRva(imports.VirtualAddress, imports.Size, *nt, sections, buffer.size()))
			{
				reason = L"import directory is outside the image/file";
				return false;
			}
		}

		if (nt->OptionalHeader.NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_BASERELOC)
		{
			const auto& relocations = directories[IMAGE_DIRECTORY_ENTRY_BASERELOC];
			hasRelocations = relocations.VirtualAddress != 0 && relocations.Size != 0;
			if (!rangeWithinRva(relocations.VirtualAddress, relocations.Size, *nt, sections, buffer.size()))
			{
				reason = L"base relocation directory is outside the image/file";
				return false;
			}
		}

		return true;
	}

	bool isX64Process(DWORD pid)
	{
		const auto handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
		if (!handle)
			return false;

		BOOL isWow64 = FALSE;
		SYSTEM_INFO systemInfo{};
		GetNativeSystemInfo(&systemInfo);
		const auto result = IsWow64Process(handle, &isWow64);
		CloseHandle(handle);

		if (!result)
			return false;

		return !isWow64 && systemInfo.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64;
	}

	bool isProcessAlive(DWORD pid)
	{
		const auto handle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
		if (!handle)
			return false;

		DWORD exitCode = 0;
		const auto result = GetExitCodeProcess(handle, &exitCode) && exitCode == STILL_ACTIVE;
		CloseHandle(handle);
		return result;
	}

	DWORD waitForProcess(std::wstring_view processName)
	{
		const auto name = std::wstring(processName);
		const auto deadline = std::chrono::steady_clock::now() + kProcessWaitTimeout;
		while (std::chrono::steady_clock::now() < deadline)
		{
			if (const auto pid = mem::getProcID(name); pid != 0)
				return pid;
			std::this_thread::sleep_for(kPollInterval);
		}
		return 0;
	}

	void logMapEvent(std::wstring_view event, DWORD pid, std::wstring_view processName, std::wstring_view moduleName, NTSTATUS status, std::wstring_view detail)
	{
		std::wostringstream stream;
		stream << L"[injector] event=" << event
			<< L" pid=" << pid
			<< L" process=" << processName
			<< L" module=" << moduleName
			<< L" status=0x" << std::hex << static_cast<unsigned long>(status)
			<< L" detail=" << detail << L'\n';
		const auto message = stream.str();
		OutputDebugStringW(message.c_str());
		std::wcerr << message;
	}
}

Injector* Injector::m_inst = nullptr;

void Injector::initialize()
{
}

bool Injector::inject(std::string dllPath)
{
	g_menu->isInjecting.store(true, std::memory_order_release);
	const auto resetState = [this]
	{
		g_menu->isInjecting.store(false, std::memory_order_release);
	};

	std::vector<BYTE> buffer;
	if (!utils::readFileToMem(std::filesystem::absolute(dllPath), buffer))
	{
		OutputDebugStringW(L"[injector] event=read-failure\n");
		resetState();
		return false;
	}

	const auto targetProcess = this->isCustomProcess ? std::wstring(this->customProcessName) : std::wstring(vars::str_game_process_name);
	const auto targetModule = this->isCustomProcess ? std::wstring(this->customProcessName) : std::wstring(vars::str_game_mod_name);
	const auto success = this->map(targetProcess, targetModule, buffer);
	resetState();

	if (success && this->shouldAutoExit)
		g_menu->isMenuOn.store(false, std::memory_order_release);
	return success;
}

bool Injector::map(std::wstring_view procname, std::wstring_view modname, const std::vector<BYTE>& buffer, blackbone::eLoadFlags flags)
{
	const auto processName = std::wstring(procname);
	const auto moduleName = std::wstring(modname);
	std::wstring validationReason;
	bool hasRelocations = false;
	if (!validatePeImage(buffer, validationReason, hasRelocations))
	{
		logMapEvent(L"pe-validation-failure", 0, processName, moduleName, STATUS_INVALID_IMAGE_FORMAT, validationReason);
		return false;
	}

	const auto pid = waitForProcess(processName);
	if (pid == 0)
	{
		logMapEvent(L"process-timeout", 0, processName, moduleName, STATUS_NOT_FOUND, L"target process was not found before timeout");
		return false;
	}

	if (!isX64Process(pid))
	{
		logMapEvent(L"architecture-mismatch", pid, processName, moduleName, STATUS_INVALID_IMAGE_FORMAT, L"target is not an accessible x64 process");
		return false;
	}

	blackbone::Process proc;
	const auto attachStatus = proc.Attach(pid, kMapProcessAccess);
	if (!NT_SUCCESS(attachStatus))
	{
		logMapEvent(L"attach-failure", pid, processName, moduleName, attachStatus, L"Process::Attach failed");
		return false;
	}

	std::atomic_bool mappingFinished{ false };
	std::jthread processMonitor([&](std::stop_token stopToken)
	{
		while (!stopToken.stop_requested() && !mappingFinished.load(std::memory_order_acquire))
		{
			if (!isProcessAlive(pid))
			{
				mappingFinished.store(true, std::memory_order_release);
				logMapEvent(L"target-exited", pid, processName, moduleName, STATUS_PROCESS_IS_TERMINATING, L"target exited while waiting for module readiness");
				return;
			}
			std::this_thread::sleep_for(kPollInterval);
		}
	});

	const auto moduleDeadline = std::chrono::steady_clock::now() + kModuleWaitTimeout;
	const auto normalizedModuleName = string::toLower(moduleName);
	bool modReady = false;
	logMapEvent(L"module-scan-start", pid, processName, moduleName, STATUS_SUCCESS, L"waiting for target module");
	while (std::chrono::steady_clock::now() < moduleDeadline)
	{
		if (mappingFinished.load(std::memory_order_acquire))
		{
			logMapEvent(L"module-wait-aborted", pid, processName, moduleName, STATUS_PROCESS_IS_TERMINATING, L"target exited");
			processMonitor.request_stop();
			proc.Detach();
			return false;
		}

		const auto& modules = proc.modules().GetAllModules();
		logMapEvent(L"module-scan", pid, processName, moduleName, STATUS_SUCCESS,
			modules.empty() ? L"module list is empty" : L"module list received");
		for (const auto& mod : modules)
		{
			if (string::toLower(mod.first.first) == normalizedModuleName)
			{
				modReady = true;
				break;
			}
		}
		if (modReady)
			break;

		std::this_thread::sleep_for(1s);
	}

	if (!modReady)
	{
		logMapEvent(L"module-timeout", pid, processName, moduleName, STATUS_TIMEOUT, L"target module was not observed before timeout");
		processMonitor.request_stop();
		proc.Detach();
		return false;
	}

	if (!hasRelocations)
		logMapEvent(L"relocation-warning", pid, processName, moduleName, STATUS_SUCCESS, L"image has no base relocation directory; rebasing may fail");

	const auto modCallback = [](blackbone::CallbackType type, void*, blackbone::Process&, const blackbone::ModuleData& modInfo)
	{
		if (type == blackbone::PreCallback && modInfo.name == L"user32.dll")
			return blackbone::LoadData(blackbone::MT_Native, blackbone::Ldr_Ignore);
		return blackbone::LoadData(blackbone::MT_Default, blackbone::Ldr_Ignore);
	};

	logMapEvent(L"map-start", pid, processName, moduleName, STATUS_SUCCESS, L"calling MapImage");
	const auto result = proc.mmap().MapImage(buffer.size(), const_cast<BYTE*>(buffer.data()), false, flags, modCallback);
	const auto status = result.status;
	if (!result.success())
	{
		logMapEvent(L"map-failure", pid, processName, moduleName, status, L"MapImage failed; rollback delegated to BlackBone");
		processMonitor.request_stop();
		proc.Detach();
		return false;
	}

	logMapEvent(L"map-success", pid, processName, moduleName, status, L"mapping completed; rollback=not-needed");
	mappingFinished.store(true, std::memory_order_release);
	processMonitor.request_stop();
	proc.Detach();
	return true;
}
