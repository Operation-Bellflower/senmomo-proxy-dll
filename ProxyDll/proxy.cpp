#include "pch.h"
#include "proxy.h"

void start_proxy()
{
	// switch to the system directory, otherwise LoadLibrary will pickup the proxy dll
	wchar_t real_dll_path[MAX_PATH];
	GetSystemDirectory(real_dll_path, MAX_PATH);
	wcscat_s(real_dll_path, L"\\d3d9.dll");

	const HMODULE h_module = LoadLibrary(real_dll_path);
	if (h_module == nullptr)
	{
		MessageBox(nullptr, L"d3d9.dll original library not found", L"Proxy", MB_ICONERROR);
		ExitProcess(0);
	}

	const auto proc_address = GetProcAddress(h_module, "Direct3DCreate9");
	original_Direct3DCreate9 = reinterpret_cast<decltype(original_Direct3DCreate9)>(proc_address);
}

// function exported by the DLL, simply redirects back to the original function in d3d9.dll
__declspec(naked) void fake_Direct3DCreate9() { _asm { jmp[original_Direct3DCreate9] } }
