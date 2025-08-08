#include "pch.h"

#include <filesystem>
#include <fstream>

#include "hooks.h"
#include "string_utils.h"

decltype(TextOutA)* real_TextOutA;
std::map<BYTE, std::map<BYTE, nlohmann::json>> mappings{};

// https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-textouta
BOOL WINAPI fake_TextOutA(HDC hdc, int x, int y, LPCSTR lp_string, int c)
{
	//std::cout << "TextOutA, string: " << lp_string << " length: " << c << " hex: ";
	//for (int i = 0; i < c; i++)
	//	std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(lp_string[i])) << " ";
	//std::cout << std::endl;

	if (c != 2)
		return real_TextOutA(hdc, x, y, lp_string, c);

	const unsigned char high_byte = lp_string[0];
	const unsigned char low_byte = lp_string[1];

	if (mappings.contains(high_byte) && mappings[high_byte].contains(low_byte))
	{
		const nlohmann::json mapping = mappings[high_byte][low_byte];
		if (mapping.contains("Replacement"))
		{
			const std::string replacement = mapping["Replacement"].get<std::string>();
			const std::wstring replacement_wide = utf8_to_wide(replacement);
			//std::cout << "Replace: " << std::hex << std::setw(2) << std::setfill('0')  << (int)high_byte << " " << (int)low_byte << std::endl;

			const auto previous_font_handle = GetCurrentObject(hdc, OBJ_FONT);
			LOGFONTA font;
			GetObjectA(previous_font_handle, sizeof(font), &font);

			if (mapping.contains("Height"))
				font.lfHeight = mapping["Height"].get<int>();
			if (mapping.contains("Width"))
				font.lfWidth = mapping["Width"].get<int>();
			if (mapping.contains("Bold"))
				font.lfWeight = mapping["Bold"].get<bool>() ? FW_BOLD : FW_NORMAL;
			if (mapping.contains("Italic"))
				font.lfItalic = mapping["Italic"].get<bool>();
			if (mapping.contains("Underline"))
				font.lfUnderline = mapping["Underline"].get<bool>();
			const auto new_font_handle = CreateFontIndirectA(&font);
			SelectObject(hdc, new_font_handle);

			const auto result = TextOutW(hdc, x, y, replacement_wide.c_str(), static_cast<int>(replacement_wide.length()));

			SelectObject(hdc, previous_font_handle);
			DeleteObject(new_font_handle);

			return result;
		}
	}

	return real_TextOutA(hdc, x, y, lp_string, c);
}

void init_sjis_ext()
{
	const HMODULE h_exe = GetModuleHandle(nullptr);
	wchar_t exe_path[MAX_PATH];
	GetModuleFileName(h_exe, exe_path, std::size(exe_path));
	std::filesystem::path sjis_ext_path(exe_path);
	sjis_ext_path = sjis_ext_path.parent_path() / "Patch" / "sjis_ext.json";
	std::ifstream stream(sjis_ext_path);

	if (stream.fail())
		MessageBox(nullptr, L"Could not find sjis_ext.json, some text will not display properly", L"Proxy",
		           MB_ICONERROR);
	else
	{
		nlohmann::json sjis_ext;
		stream >> sjis_ext;

		for (auto& mapping : sjis_ext.items())
		{
			std::istringstream key_stream(mapping.key());
			unsigned int highByte; // uses int rather than char because otherwise the wrong overload of >> will be used
			unsigned int lowByte;
			key_stream >> std::hex >> highByte;
			key_stream >> std::hex >> lowByte;
			mappings[highByte][lowByte] = mapping.value();
		}
	}

	hook("gdi32.dll",
	     "TextOutA",
	     fake_TextOutA,
	     reinterpret_cast<PDETOUR_TRAMPOLINE*>(&real_TextOutA));

	std::cout << "Special character and formatting support initialized" << std::endl;
}
