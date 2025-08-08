#include "pch.h"
#include "hooks.h"
#include "string_utils.h"

const std::string serif_font = wide_string_to_narrow(L"SimSun");

// https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createfonta
decltype(CreateFontA)* real_CreateFontA;

HFONT WINAPI fake_CreateFontA(int c_height, int c_width, int c_escapement, int c_orientation, int c_weight,
	DWORD b_italic, DWORD b_underline, DWORD b_strike_out, DWORD i_char_set,
	DWORD i_out_precision, DWORD i_clip_precision, DWORD i_quality, DWORD i_pitch_and_family,
	LPCSTR psz_face_name)
{
	//std::cout << "CreateFontA" << std::endl;

	if (strcmp(psz_face_name, serif_font.c_str()) == 0)
	{
		c_weight = FW_BLACK;
		c_width = static_cast<int>(std::round(c_width * 0.75)); // Adjust width to be the same as the sans-serif font
	}

	return real_CreateFontA(c_height, c_width, c_escapement, c_orientation, c_weight, b_italic, b_underline,
		b_strike_out, i_char_set, i_out_precision, i_clip_precision, i_quality, i_pitch_and_family,
		psz_face_name);
}

// https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createfontindirecta
decltype(CreateFontIndirectA)* real_CreateFontIndirectA;

HFONT WINAPI fake_CreateFontIndirectA(const LOGFONTA* lplf)
{
	//std::cout << "CreateFontIndirectA" << std::endl;
	return real_CreateFontIndirectA(lplf);
}

void hook(LPCSTR module_handle, LPCSTR lp_proc_name, PVOID p_detour, PDETOUR_TRAMPOLINE* pp_trampoline)
{
	const HMODULE h_module = GetModuleHandleA(module_handle);
	if (h_module == nullptr)
	{
		const std::string message = "Couldn't find module: " + std::string(module_handle);
		MessageBoxA(nullptr, message.c_str(), "Proxy", MB_ICONERROR);
		return;
	}

	LPVOID p_real_function = GetProcAddress(h_module, lp_proc_name);
	if (p_real_function == nullptr)
	{
		const std::string message = "Couldn't find function: " + std::string(lp_proc_name)
			+ " in module: " + std::string(module_handle);
		MessageBoxA(nullptr, message.c_str(), "Proxy", MB_ICONERROR);
		return;
	}

	DetourTransactionBegin();
	DetourUpdateThread(GetCurrentThread());
	DetourAttachEx(&p_real_function, p_detour, pp_trampoline, nullptr, nullptr);
	DetourTransactionCommit();
}

void attach_hooks()
{
	hook(
		"gdi32.dll",
		"CreateFontA",
		fake_CreateFontA,
		reinterpret_cast<PDETOUR_TRAMPOLINE*>(&real_CreateFontA));

	hook("gdi32.dll",
		"CreateFontIndirectA",
		fake_CreateFontIndirectA,
		reinterpret_cast<PDETOUR_TRAMPOLINE*>(&real_CreateFontIndirectA));
}
