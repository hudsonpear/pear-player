; Inno Setup script for Pear Player.
;
; Not built directly -- run tools\make-installer.ps1, which stages the runtime
; files into installer\staging first and then invokes ISCC on this script.
; Staging matters: build\ also holds CMake and Ninja artifacts (object files,
; CMakeCache.txt, the autogen tree) that must not ship.
;
; Keep AppVersion in step with main.cpp's setApplicationVersion() and the
; version block in app.rc.

#define AppName        "Pear Player"
#define AppVersion     "1.0.0"
#define AppPublisher   "Hudson Pear"
#define AppUrl         "https://github.com/hudsonpear/pear-player"
#define AppExeName     "PearPlayer.exe"

[Setup]
; A fixed GUID identifies the application across versions -- upgrades replace
; the previous install instead of piling up beside it. Never change it once
; released, or upgrades stop being recognised as upgrades.
AppId={{7C4F1A62-9E3B-4C8D-9A21-5B6E0D7F3C14}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}
AppUpdatesURL={#AppUrl}

DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
LicenseFile=
OutputDir=..\dist
OutputBaseFilename=PearPlayer-{#AppVersion}-Setup
SetupIconFile=..\resources\pearicon.ico
UninstallDisplayIcon={app}\{#AppExeName}
WizardStyle=modern

; Stamps the setup .exe itself. Without these its own Properties panel shows a
; blank version, which looks unsigned-and-unknown to users checking a download
; before running it.
VersionInfoVersion={#AppVersion}
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} {#AppVersion} Setup
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}

; libmpv alone is ~117 MB, so a strong compressor is worth the build time.
Compression=lzma2/max
SolidCompression=yes

; The app is 64-bit only; this also puts it in Program Files rather than
; Program Files (x86).
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

; Installing to Program Files needs elevation. Users who would rather not
; elevate can pick a per-user directory in the wizard.
PrivilegesRequiredOverridesAllowed=dialog

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
; Both ticked by default. The user still sees them on the wizard's Additional
; Tasks page and can clear either one before installing.
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "associate"; Description: "Open video and audio files with {#AppName}"; GroupDescription: "File associations:"

[Files]
; Everything staged by make-installer.ps1. recursesubdirs picks up the Qt
; plugin folders (platforms, imageformats, styles, tls, ...), which the app
; will not start without.
Source: "staging\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{group}\{cm:UninstallProgram,{#AppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

[Registry]
; Registered under Capabilities so the app shows up in "Open with" and in the
; Default Apps panel, which is the supported route on Windows 10/11 -- writing
; the file extensions directly no longer makes an app the default.
Root: HKA; Subkey: "Software\{#AppPublisher}\{#AppName}\Capabilities"; ValueType: string; ValueName: "ApplicationName"; ValueData: "{#AppName}"; Tasks: associate; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\{#AppPublisher}\{#AppName}\Capabilities"; ValueType: string; ValueName: "ApplicationDescription"; ValueData: "Video and audio player"; Tasks: associate
Root: HKA; Subkey: "Software\RegisteredApplications"; ValueType: string; ValueName: "{#AppName}"; ValueData: "Software\{#AppPublisher}\{#AppName}\Capabilities"; Tasks: associate; Flags: uninsdeletevalue

; The ProgIDs themselves are written from [Code]: there is one per extension
; (see RegisterExtension), which is far too many to spell out here, and they
; are removed again in CurUninstallStepChanged.

; Listing the app under Applications\ is what puts it in Explorer's "Open with"
; menu, which is a separate mechanism from the Capabilities above: Capabilities
; offers the app as a *default* handler, while this offers it as *a* choice.
; Registered whether or not the associations task was ticked -- it takes nothing
; over, it only makes the player available to pick.
Root: HKA; Subkey: "Software\Classes\Applications\{#AppExeName}"; ValueType: string; ValueName: "FriendlyAppName"; ValueData: "{#AppName}"; Flags: uninsdeletekey
Root: HKA; Subkey: "Software\Classes\Applications\{#AppExeName}\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#AppExeName}"" ""%1"""

[Run]
Filename: "{app}\{#AppExeName}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[Code]
// The file types offered under Capabilities, split by family so each points at
// the ProgID carrying the right icon. Kept in one place so the lists match the
// extensions the player actually accepts.
const
  VideoExtensions = '.mp4/.mkv/.avi/.mov/.webm/.flv/.wmv/.m4v/.mpg/.mpeg/.m2v/.ts/.m2ts/.mts/.vob/.3gp/.3g2/.ogv';
  AudioExtensions = '.mp3/.wav/.flac/.aac/.ogg/.opus/.m4a/.wma/.mka/.aiff/.ac3/.dts/.amr';
  ImageExtensions = '.jpg/.jpeg/.png/.webp/.gif';

// The ProgID for one extension: '.mp4' -> 'PearPlayer.mp4'.
function ProgIdFor(const Extension: string): string;
begin
  Result := 'PearPlayer' + Lowercase(Extension);
end;

// Registers one ProgID per extension rather than one per family.
//
// Explorer's Type column shows the ProgID's default value, so a single shared
// ProgID makes every video read the same thing -- an .mkv and an .avi both
// showing "Video File", with no way to tell formats apart in a folder. One
// ProgID each gives Windows something specific to show ("MKV Video File"),
// which is what the built-in handlers do.
//
// IconIndex points into the extra ICON resources app.rc compiles into the exe:
//   1 = video   2 = audio   3 = image   (0 is the plain app icon)
procedure RegisterExtension(const Extension, TypeSuffix, CapKey: string; IconIndex: Integer);
var
  ProgId, Key, Exe, TypeName: string;
begin
  ProgId := ProgIdFor(Extension);
  Key := 'Software\Classes\' + ProgId;
  Exe := ExpandConstant('{app}\{#AppExeName}');
  // ".mp4" -> "MP4 Video File", matching how Windows names its own types.
  TypeName := Uppercase(Copy(Extension, 2, Length(Extension) - 1)) + ' ' + TypeSuffix;

  RegWriteStringValue(HKA, Key, '', TypeName);
  RegWriteStringValue(HKA, Key + '\DefaultIcon', '', Exe + ',' + IntToStr(IconIndex));
  RegWriteStringValue(HKA, Key + '\shell\open\command', '', '"' + Exe + '" "%1"');

  // The Capabilities entry points the extension at that same ProgID, which is
  // what the Default Apps panel acts on.
  RegWriteStringValue(HKA, CapKey, Extension, ProgId);
end;

procedure RegisterFamily(const Extensions, TypeSuffix, CapKey: string; IconIndex: Integer);
var
  Items: TArrayOfString;
  I: Integer;
begin
  Items := StringSplitEx(Extensions, ['/'], #0, stExcludeEmpty);
  for I := 0 to GetArrayLength(Items) - 1 do
    RegisterExtension(Items[I], TypeSuffix, CapKey, IconIndex);
end;

procedure RegisterMediaCapabilities;
var
  CapKey: string;
begin
  CapKey := 'Software\{#AppPublisher}\{#AppName}\Capabilities\FileAssociations';
  RegisterFamily(VideoExtensions, 'Video File', CapKey, 1);
  RegisterFamily(AudioExtensions, 'Audio File', CapKey, 2);
  RegisterFamily(ImageExtensions, 'Image File', CapKey, 3);
end;

// Every type the player accepts, listed under the app's own Applications key.
// Explorer reads this to decide which apps to offer in "Open with", so a format
// missing here means the player does not appear for it even though it can play
// it -- which is exactly what happened to .3gp and .ts.
procedure RegisterSupportedTypes;
var
  Items: TArrayOfString;
  Key, All: string;
  I: Integer;
begin
  Key := 'Software\Classes\Applications\{#AppExeName}\SupportedTypes';
  All := VideoExtensions + '/' + AudioExtensions + '/' + ImageExtensions;
  Items := StringSplitEx(All, ['/'], #0, stExcludeEmpty);
  for I := 0 to GetArrayLength(Items) - 1 do
    RegWriteStringValue(HKA, Key, Items[I], '');
end;

// Versions up to 1.0.0 used one ProgID per family, which is why Explorer showed
// every video as plain "Video File". Those keys are left over on an upgrade,
// and any association still pointing at one would keep showing the old name,
// so they go.
procedure RemoveLegacyProgIds;
begin
  RegDeleteKeyIncludingSubkeys(HKA, 'Software\Classes\PearPlayer.Video');
  RegDeleteKeyIncludingSubkeys(HKA, 'Software\Classes\PearPlayer.Audio');
  RegDeleteKeyIncludingSubkeys(HKA, 'Software\Classes\PearPlayer.Image');
end;

procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    RemoveLegacyProgIds;
    RegisterSupportedTypes;
    if WizardIsTaskSelected('associate') then
      RegisterMediaCapabilities;
  end;
end;

// The per-extension ProgIDs are written from code, so Inno's uninsdeletekey
// never sees them and they would otherwise be left behind -- leaving Explorer
// showing "MP4 Video File" for a player that is no longer installed.
procedure UnregisterProgIds;
var
  Items: TArrayOfString;
  All: string;
  I: Integer;
begin
  All := VideoExtensions + '/' + AudioExtensions + '/' + ImageExtensions;
  Items := StringSplitEx(All, ['/'], #0, stExcludeEmpty);
  for I := 0 to GetArrayLength(Items) - 1 do
    RegDeleteKeyIncludingSubkeys(HKA, 'Software\Classes\' + ProgIdFor(Items[I]));
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
begin
  if CurUninstallStep = usUninstall then
    UnregisterProgIds;
end;
