# PearPlayer

Qt6 Widgets + libmpv video player. Builds to `build/PearPlayer.exe`.

## Always build after finishing changes

Every time a change is finished, build it before reporting back. Do not hand
over code that has not been through the compiler.

```powershell
$env:PATH = "C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\mingw1310_64\bin;$env:PATH"
cmake --build build
```

`cmake` and `ninja` are not on the system PATH, hence the prefix above. The
generator is Ninja, the compiler is the MinGW 13.1 toolchain shipped with
Qt 6.10.1, and the RC compiler is that toolchain's `windres`.

If CMakeLists.txt changed, configure first:

```powershell
cmake -G Ninja -S . -B build
```

### Build gotchas

- **`ld.exe: cannot open output file PearPlayer.exe: Permission denied`**
  means the player is still running and holding its own exe. Check with
  `Get-Process PearPlayer`; ask before killing it, since it may be playing
  something.
- **`does not match the source ... used to generate cache`** means `build/`
  holds a cache from a previous path (this folder used to be named
  `VideoPlayerAPP`). Delete `build/CMakeCache.txt` and `build/CMakeFiles/`,
  then configure again. Nothing else in `build/` needs to go.
- `windeployqt` runs as a POST_BUILD step, so Qt DLLs and plugins land next
  to the exe automatically. libmpv is copied from `third_party/mpv/`.

## Icons

`pearicon.png` in the repo root is the master artwork. Everything else is
generated from it by `tools/make-icons.ps1`:

- `resources/pearicon.ico` — compiled into the exe through `app.rc`
- `resources/icons/pearicon-<n>.png` — embedded through `app.qrc`, loaded in
  `main.cpp` as the window icon

Rerun the script after editing the master art; the outputs are committed so a
normal build never needs PowerShell.

If Explorer shows a stale icon after a rebuild, that is the shell icon cache:
`ie4uinit.exe -show`.

## Accent color

The sliders use the pear's own greens, defined once in `Theme.h` as
`Theme::kAccentStart` (184,217,50) and `Theme::kAccentEnd` (130,179,36). Use
those rather than writing literal colors into a widget, so the timeline and
volume tracks cannot drift apart.
