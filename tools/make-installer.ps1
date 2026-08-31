# Builds the Windows installer.
#
#   pwsh -File tools/make-installer.ps1
#
# Stages the runtime files out of build/ into installer/staging, then runs the
# Inno Setup compiler on installer/PearPlayer.iss. The finished installer lands
# in dist/.
#
# Staging exists because build/ holds far more than the app: CMake caches,
# Ninja logs, object files and the autogen tree. Shipping build/ wholesale
# would roughly double the download and leak local paths.

param(
    [string]$BuildDir = (Join-Path $PSScriptRoot ".." | Join-Path -ChildPath "build"),
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"

$root       = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$staging    = Join-Path $root "installer\staging"
$script     = Join-Path $root "installer\PearPlayer.iss"
$distDir    = Join-Path $root "dist"

# --- Compile first, unless told otherwise -------------------------------------
if (-not $SkipBuild) {
    $env:PATH = "C:\Qt\Tools\CMake_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\mingw1310_64\bin;$env:PATH"

    # A running player holds its own exe and the link fails silently enough to
    # be mistaken for a build that did nothing.
    if (Get-Process PearPlayer -ErrorAction SilentlyContinue) {
        throw "PearPlayer is running; close it before building the installer."
    }

    Write-Host "Building..." -ForegroundColor Cyan
    cmake --build $BuildDir
    if ($LASTEXITCODE -ne 0) { throw "Build failed." }
}

$exe = Join-Path $BuildDir "PearPlayer.exe"
if (-not (Test-Path $exe)) { throw "No PearPlayer.exe in $BuildDir." }

# --- Stage the runtime files ---------------------------------------------------
Write-Host "Staging runtime files..." -ForegroundColor Cyan
if (Test-Path $staging) { Remove-Item $staging -Recurse -Force }
New-Item -ItemType Directory -Force $staging | Out-Null

# Build-system output, none of which belongs in the installer.
$skipDirs  = @("CMakeFiles", "VideoPlayerApp_autogen", ".qt", "Testing")
$skipFiles = @("CMakeCache.txt", ".ninja_deps", ".ninja_log", "build.ninja",
                "cmake_install.cmake", "libPearPlayer.dll.a")

Get-ChildItem $BuildDir -Force | Where-Object {
    if ($_.PSIsContainer) {
        $skipDirs -notcontains $_.Name
    } else {
        ($skipFiles -notcontains $_.Name) -and
        ($_.Extension -notin ".obj", ".a", ".ninja", ".cmake", ".stamp")
    }
} | Copy-Item -Destination $staging -Recurse -Force

$staged = Get-ChildItem $staging -Recurse -File
$sizeMb = ($staged | Measure-Object Length -Sum).Sum / 1MB
Write-Host ("  {0} files, {1:N1} MB" -f $staged.Count, $sizeMb)

# Catch a missing deployment before it becomes a broken installer: the Qt
# platform plugin is the one whose absence stops the app dead at startup.
$platformPlugin = Join-Path $staging "platforms\qwindows.dll"
if (-not (Test-Path $platformPlugin)) {
    throw "platforms\qwindows.dll is missing from the staged files -- windeployqt did not run."
}

# --- Compile the installer ------------------------------------------------------
# The per-user path is listed too: winget installs Inno Setup under LOCALAPPDATA
# rather than Program Files unless setup is run elevated.
$iscc = @(
    "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
    "$env:ProgramFiles\Inno Setup 6\ISCC.exe",
    "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
) | Where-Object { Test-Path $_ } | Select-Object -First 1

if (-not $iscc) {
    throw "Inno Setup 6 not found. Install it with:  winget install JRSoftware.InnoSetup"
}

New-Item -ItemType Directory -Force $distDir | Out-Null
Write-Host "Compiling installer..." -ForegroundColor Cyan
& $iscc $script
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed." }

Get-ChildItem $distDir -Filter "*.exe" | ForEach-Object {
    Write-Host ("`nInstaller: {0} ({1:N1} MB)" -f $_.FullName, ($_.Length / 1MB)) -ForegroundColor Green
}
