<p align="center">
   <img src="pearicon.png" width="96" align="center" alt="Pear Player icon">


# <p align="center">Pear Player</p>

<p align="center">
   A Windows video player built on Qt 6 Widgets and libmpv. Frameless custom
   title bar, per-file playback memory, and mpv's full decoding stack behind a
   small interface.
</p>

## Features

- Plays anything mpv plays — MP4, MKV, WebM, AVI, TS, MP3, FLAC, Opus and the rest
- Open a file, a whole folder, or a URL; drag and drop from Explorer
- Download from a URL, or download the media currently playing
- Playlist with loop off / loop file / loop playlist, chapter navigation
- Frame stepping and speed control
- Subtitle track selection, external subtitle files, and subtitle styling
- Audio track and device selection, channel layout, audio delay adjustment
- 10-band equalizer
- Video rotate, flip, mirror, zoom, aspect ratio, and picture positioning
- Media information dialog
- Remembers position, volume, and track choices per file
- Single instance: opening a second file hands it to the running window
- Taskbar progress, recent files, fullscreen
- Interface translations — English, Portuguese (Brazil), Spanish, French, German

## Requirements

| | |
|---|---|
| OS | Windows 10 or later, 64-bit |
| Qt | 6.x with the MinGW 64-bit toolchain (tested on Qt 6.10.1 / MinGW 13.1) |
| CMake | 3.21 or later, plus Ninja |
| libmpv | `mpv-dev` package — **not included in this repository**, see below |

## libmpv-2.dll is not in this repository

`third_party/mpv/libmpv-2.dll` is about **117 MB**, which is over GitHub's
100 MB per-file limit, so it is git-ignored. The build **will not link and the
player will not start** until you put it there yourself. The headers
(`third_party/mpv/include/mpv/`) and the import library
(`third_party/mpv/libmpv.dll.a`) *are* committed, so the DLL is the only
missing piece.

To get it:

1. Download an `mpv-dev-x86_64-*.7z` build from
   [shinchiro/mpv-winbuild-cmake releases](https://github.com/shinchiro/mpv-winbuild-cmake/releases)
   (the `mpv-dev` package, not the player build).
2. Extract `libmpv-2.dll` from it.
3. Drop it into `third_party/mpv/` so the folder looks like this:

   ```
   third_party/mpv/
     libmpv-2.dll        <- the file you just added
     libmpv.dll.a
     include/mpv/client.h, render.h, render_gl.h, stream_cb.h
   ```

If the DLL lives somewhere else already, point CMake at its folder instead:
`cmake -G Ninja -S . -B build -DMPV_ROOT="D:/path/to/mpv-dev"`.

CMake fails outright when the headers or the import library are missing, and
warns — but keeps going — when only the runtime DLL is absent. That warning is
the one to watch for: the build succeeds and the executable then fails to
launch. The DLL is copied next to `PearPlayer.exe` as a post-build step.

## Building

```powershell
cmake -G Ninja -S . -B build
cmake --build build
```

`cmake` and `ninja` ship with Qt but are not on the system PATH; put the
toolchain on it first if needed:

```powershell
$env:PATH = "C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\mingw1310_64\bin;$env:PATH"
```

CMake looks for Qt under `C:/Qt/*/mingw_64` on its own; pass
`-DCMAKE_PREFIX_PATH=...` if yours lives elsewhere. `windeployqt` runs as a
post-build step, so the Qt DLLs and plugins land beside the executable
automatically.

The result is `build/PearPlayer.exe`.

### If the link fails with "Permission denied"

`ld.exe: cannot open output file PearPlayer.exe: Permission denied` means a
copy of the player is still running and holding its own executable. Close it
and build again.

## Installer

`tools/make-installer.ps1` drives [Inno Setup](https://jrsoftware.org/isinfo.php)
over `installer/PearPlayer.iss`, staging the executable together with the Qt
and mpv runtime into `installer/staging/`, and writes the setup executable to
`dist/`. Both of those folders are git-ignored — they are build products.

```powershell
pwsh -File tools/make-installer.ps1
```

## Translations

Each language is one UTF-8 JSON file in `translations/`, picked up at startup
and listed under Settings > Interface > Language. No compiling and no tooling
involved — a text editor is enough. See
[translations/README.txt](translations/README.txt) for the format, and copy
`translations/template.json` to start a new one.

## Icons

`pearicon.png` in the repository root is the master artwork. Everything under
`resources/` is generated from it by `tools/make-icons.ps1` and committed, so a
normal build never needs to run that script. Rerun it after editing the master
art.

If Explorer keeps showing a stale icon after a rebuild, that is the shell icon
cache: `ie4uinit.exe -show`.

## Project layout

```
*.cpp / *.h            application sources
CMakeLists.txt         build definition
app.qrc, app.rc        embedded icons and Windows version info
pearicon.png           master icon artwork
resources/             generated .ico and .png icon sizes
translations/          language JSON files + template
third_party/mpv/       libmpv headers and import lib (DLL not committed)
tools/                 icon, installer, and translation-template scripts
installer/             Inno Setup script (staging/ is generated)
```

## Credits

Playback is [mpv](https://mpv.io/)'s; the interface is Qt 6 Widgets. Pear
Player 1.0.1 — Hudson Pear, 2026.
