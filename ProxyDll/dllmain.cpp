#include "pch.h"
#include "console.h"
#include "proxy.h"
#include "hooks.h"
#include "locale_emulator.h"
#include "mpv_integration.h"
#include "sjis_ext.h"

BOOL APIENTRY DllMain(HMODULE h_module, DWORD  ul_reason_for_call, LPVOID lp_reserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		start_proxy();
		// open_console(); // Displays the DLL's std output in a console window, helpful for debugging

		if (GetACP() != 932 && locale_emulator::relaunch())
			ExitProcess(0);

		//MessageBox(nullptr, L"Starting patch...", L"Proxy", MB_ICONINFORMATION);
		attach_hooks();
		init_sjis_ext();
		init_mpv_integration();
		std::cout << "Patch DLL initialization complete" << std::endl;

		break;
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
	default:
		break;
	}
	return TRUE;
}

