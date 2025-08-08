#include "pch.h"

void open_console()
{
	AllocConsole();
	SetConsoleOutputCP(932);
	freopen_s(reinterpret_cast<FILE**>(stdout), "CONOUT$", "w", stdout);
	std::cout << "[Senmomo patch debug window]" << std::endl;
}
