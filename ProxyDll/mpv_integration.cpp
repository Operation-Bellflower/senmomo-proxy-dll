#include "pch.h"

#define CINTERFACE
#include <dshow.h>
#include <filesystem>
#include <thread>

#include "hooks.h"
#pragma comment(lib, "strmiids.lib")


mpv_handle* mpv;
IMediaControl* media_control;
std::string video_path;
HWND window_handle = nullptr;

HRESULT(WINAPI* real_IGraphBuilderQueryInterface)(IGraphBuilder* This, REFIID riid, void** ppvObject) = nullptr;
HRESULT(WINAPI* real_IMediaControlRun)(IMediaControl* This) = nullptr;
HRESULT(WINAPI* real_IMediaControlStop)(IMediaControl* This) = nullptr;
ULONG(WINAPI* real_IMediaControlRelease)(IMediaControl* This) = nullptr;

void skip_real_video()
{
	if (media_control == nullptr || real_IMediaControlRun == nullptr)
		return;

	HRESULT hr = real_IMediaControlRun(media_control);
	if (FAILED(hr))
		return;

	// Seek to 0 and end at 0 to trick the engine into thinking that the video ended
	IMediaSeeking* p_seeking;
	REFERENCE_TIME rt_now = 0;
	REFERENCE_TIME rt_stop = 0;

	hr = media_control->lpVtbl->QueryInterface(media_control, IID_IMediaSeeking, reinterpret_cast<void**>(&p_seeking));
	if (FAILED(hr))
		return;

	p_seeking->lpVtbl->SetPositions(p_seeking, &rt_now, AM_SEEKING_AbsolutePositioning, &rt_stop,
		AM_SEEKING_AbsolutePositioning);
	p_seeking->lpVtbl->Release(p_seeking);

	media_control = nullptr;
}

void play_video()
{
	std::cout << "Playing video through MPV" << std::endl;
	mpv = mpv_create();
	int64_t wid = reinterpret_cast<intptr_t>(window_handle);
	mpv_set_option(mpv, "wid", MPV_FORMAT_INT64, &wid);
	int val = 0;
	mpv_set_option(mpv, "osc", MPV_FORMAT_FLAG, &val);

	mpv_initialize(mpv);

	const char* cmd[] = { "loadfile", video_path.c_str(), NULL };
	mpv_command(mpv, cmd);

	// media_control becomes null when the player clicks on the window before the video ends
	while (media_control != nullptr)
	{
		const mpv_event* event = mpv_wait_event(mpv, -1);
		if (event->event_id == MPV_EVENT_END_FILE || event->event_id == MPV_EVENT_SHUTDOWN)
			break;
	}

	skip_real_video(); // alerts the engine that the video finished
	mpv_terminate_destroy(mpv);
	mpv = nullptr;
}

// https://docs.microsoft.com/en-us/windows/win32/api/control/nf-control-imediacontrol-run
HRESULT WINAPI fake_IMediaControlRun(IMediaControl* This)
{
	if (video_path.empty())
		return real_IMediaControlRun(This);

	// Spin up mpv player
	media_control = This;
	std::thread(&play_video).detach(); // Needs to play on another thread, otherwise window freezes
	return S_OK; // Tricks the engine into thinking that the video started
}

// https://docs.microsoft.com/en-us/windows/win32/api/control/nf-control-imediacontrol-stop
HRESULT WINAPI fake_IMediaControlStop(IMediaControl* This)
{
	media_control = nullptr;
	if (mpv != nullptr)
		mpv_wakeup(mpv); // Causes the event loop in VideoHook::PlayVideo to break and destroy the mpv player
	return real_IMediaControlStop(This);
}

// https://docs.microsoft.com/en-us/windows/win32/api/unknwn/nf-unknwn-iunknown-release
ULONG WINAPI fake_IMediaControlRelease(IMediaControl* This)
{
	if (This == media_control)
	{
		media_control = nullptr;
		if (mpv != nullptr)
			mpv_wakeup(mpv);
	}
	return real_IMediaControlRelease(This);
}


// https://docs.microsoft.com/en-us/windows/win32/api/unknwn/nf-unknwn-iunknown-queryinterface(refiid_void)
HRESULT WINAPI fake_IGraphBuilderQueryInterface(IGraphBuilder* This, REFIID riid, void** ppv_object)
{
	const HRESULT hr = real_IGraphBuilderQueryInterface(This, riid, ppv_object);
	if (SUCCEEDED(hr) && riid == IID_IMediaControl && real_IMediaControlRun == nullptr)
	{
		auto p_control = static_cast<IMediaControl*>(*ppv_object);
		real_IMediaControlRun = p_control->lpVtbl->Run;
		real_IMediaControlStop = p_control->lpVtbl->Stop;
		real_IMediaControlRelease = p_control->lpVtbl->Release;

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&reinterpret_cast<PVOID&>(real_IMediaControlRun), fake_IMediaControlRun);
		DetourAttach(&reinterpret_cast<PVOID&>(real_IMediaControlStop), fake_IMediaControlStop);
		DetourAttach(&reinterpret_cast<PVOID&>(real_IMediaControlRelease), fake_IMediaControlRelease);
		DetourTransactionCommit();
	}
	return hr;
}

// https://docs.microsoft.com/en-us/windows/win32/api/combaseapi/nf-combaseapi-cocreateinstance
decltype(CoCreateInstance)* real_CoCreateInstance;

HRESULT WINAPI fake_CoCreateInstance(REFCLSID rclsid, LPUNKNOWN p_unk_outer, DWORD dw_cls_context, REFIID riid,
	LPVOID* ppv)
{
	const HRESULT hr = real_CoCreateInstance(rclsid, p_unk_outer, dw_cls_context, riid, ppv);
	if (SUCCEEDED(hr) && rclsid == CLSID_FilterGraph && real_IGraphBuilderQueryInterface == nullptr)
	{
		auto p_graph = static_cast<IGraphBuilder*>(*ppv);
		real_IGraphBuilderQueryInterface = p_graph->lpVtbl->QueryInterface;

		DetourTransactionBegin();
		DetourUpdateThread(GetCurrentThread());
		DetourAttach(&reinterpret_cast<PVOID&>(real_IGraphBuilderQueryInterface), fake_IGraphBuilderQueryInterface);
		DetourTransactionCommit();
	}
	return hr;
}

// https://docs.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-getfileattributesa
// Used to get the filename of videos that the engine wants to play
decltype(GetFileAttributesA)* real_GetFileAttributesA;

DWORD WINAPI fake_GetFileAttributesA(LPCSTR lp_file_name)
{
	const auto path = std::filesystem::path(lp_file_name);
	const auto exe_path = std::filesystem::current_path();
	const auto patch_path = std::filesystem::path(R"(.\Patch\Videos)");

	// Checks that we're looking for a video in the game's folder
	if (path.extension().compare(".mpg") == 0 && path.parent_path().compare(exe_path) == 0)
	{
		auto replacement_video = patch_path / path.filename();
		replacement_video.replace_extension(".mkv");
		if (exists(replacement_video))
		{
			std::cout << "Set video path to: " << video_path << std::endl;
			video_path = replacement_video.string(); // Video was converted to MKV. Use MPV
		}
		else
			video_path = std::string(); // Video hasn't been replaced. Play through the old player
	}

	return real_GetFileAttributesA(lp_file_name);
}

// https://docs.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-createwindowexa
decltype(CreateWindowExA)* real_CreateWindowExA;

HWND WINAPI fake_CreateWindowExA(DWORD dw_ex_style, LPCSTR lp_class_name, LPCSTR lp_window_name, DWORD dw_style,
	int x, int y, int n_width, int n_height, HWND h_wnd_parent, HMENU h_menu,
	HINSTANCE h_instance, LPVOID lp_param)
{
	HWND result = real_CreateWindowExA(dw_ex_style, lp_class_name, lp_window_name, dw_style, x, y, n_width,
		n_height, h_wnd_parent, h_menu, h_instance, lp_param);
	if (window_handle == nullptr)
		window_handle = result;
	return result;
}

void init_mpv_integration()
{
	hook("ole32.dll",
		"CoCreateInstance",
		fake_CoCreateInstance,
		reinterpret_cast<PDETOUR_TRAMPOLINE*>(&real_CoCreateInstance));

	hook("kernel32.dll",
		"GetFileAttributesA",
		fake_GetFileAttributesA,
		reinterpret_cast<PDETOUR_TRAMPOLINE*>(&real_GetFileAttributesA));

	hook("user32.dll",
		"CreateWindowExA",
		fake_CreateWindowExA,
		reinterpret_cast<PDETOUR_TRAMPOLINE*>(&real_CreateWindowExA));

	std::cout << "MPV integration initialized" << std::endl;
}
