#pragma once

void hook(LPCSTR module_handle, LPCSTR lp_proc_name, PVOID p_detour, PDETOUR_TRAMPOLINE* pp_trampoline);
void attach_hooks();
