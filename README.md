# Senmomo Proxy DLL mirror

This repository is a mirror of the source code for the proxy DLL that we bundled into the Senmomo English patch, `d3d9.dll`.

## Building

Build with the `msbuild` command, available with Visual Studio. For building our patch, MSBuild version 17.4.1 was used.

Run the below command to build:

```
msbuild /m /p:Configuration=Release /v:minimal .\ProxyDll\ProxyDll.vcxproj
Copy-Item .\ProxyDll\Release\ProxyDll.dll .\d3d9.dll
```

## Overview

The `ProxyDLL` solution builds a DLL file that mimicks the interface of `d3d9.dll`, a graphics API used by the BGI engine. Placing the DLL next to the BGI executable will cause it to be loaded instead of the normal `d3d9.dll` library.

A lot of this code has been forked from [VNTranslationTools by ArcusMaximus](https://github.com/arcusmaximus/VNTranslationTools).

The proxy DLL includes the following functionalities:

- Redirecting functionality to the normal Direct3D library, allowing the engine to work as normal. See [proxy.cpp](ProxyDll/proxy.cpp)
- Relaunching the process with Japanese locale emulation, using the third-party [LocaleEmulator](https://xupefei.github.io/Locale-Emulator/) software, which needs to be provided as a dll. See [locale_emulator.cpp](ProxyDll/locale_emulator.cpp). Emulating a Japanese locale is necessary for the engine to work properly. Essentially, this is allowing us to bundle LocaleEmulator with our patch so you don't have to launch it manually.
- Hooking into various Windows APIs to alter certain behaviors of the engine. This is done using [Microsoft's Detours package](https://github.com/microsoft/Detours). In particular, we are hooking into the following APIs:
  - [CreateFontA](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createfonta) and [CreateFontIndirectA](https://learn.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-createfontindirecta) to tweak various aspects of the fonts loaded within the engine, so that it better displays text in non-Japanese alphabets.
  - [TextOutA](https://docs.microsoft.com/en-us/windows/win32/api/wingdi/nf-wingdi-textouta), the function responsible for displaying text on the screen. We use this for replacing certain characters in the script into other characters that the engine is normally not able to display, including characters with font styles (italic, bold, underlined...) or characters that are not available within the Shift JIS encoding (such as éèêë). There is a `sjis_ext.json` file included in the patch that provides the mappings of "characters in the script" to "character to display on screen". See [sjis_ext.cpp](ProxyDll/sjis_ext.cpp)
- Providing the [MPV player](https://mpv.io/) as a replacement for the game's built-in video player. As part of our patch, we've added subtitles to the game's various music videos, and re-encoded them in h265 and ASS subtitle formats in a Matroska container, which cannot be played natively by BGI. To get around this, we bundled with our patch a copy of MPV, an open-source video player. By hooking onto various APIs with Detour, we are able to let MPV run directly within the game's window and properly display our subitled videos. See [mpv_integration.cpp](ProxyDll/mpv_integration.cpp).
