#define MyAppName "StemFetch"
#define MyAppVersion "0.1.0"

[Setup]
AppId={{B6F1D2A4-7C3E-4E5B-9A10-3D2F6C8E1A55}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
DefaultDirName={autopf}\StemFetch
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=output
OutputBaseFilename=StemFetch-Setup
Compression=lzma2
SolidCompression=yes
UninstallDisplayName=StemFetch (VST3)

[Files]
; VST3 bundle -> the standard VST3 folder FL Studio scans
Source: "..\build\StemFetch_artefacts\Release\VST3\StemFetch.vst3\*"; \
    DestDir: "{commoncf64}\VST3\StemFetch.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
; Backend files -> %APPDATA%\StemFetch (where the plugin looks for them)
Source: "..\backend\fetch_and_split.py"; DestDir: "{userappdata}\StemFetch"; Flags: ignoreversion
Source: "setup_backend.ps1"; DestDir: "{userappdata}\StemFetch"; Flags: ignoreversion

[Run]
Filename: "powershell.exe"; \
    Parameters: "-NoProfile -ExecutionPolicy Bypass -File ""{userappdata}\StemFetch\setup_backend.ps1"""; \
    StatusMsg: "Installing Python, Demucs, yt-dlp and ffmpeg (a few GB - this can take several minutes)..."; \
    Flags: runasoriginaluser waituntilterminated

[UninstallDelete]
Type: filesandordirs; Name: "{userappdata}\StemFetch"
