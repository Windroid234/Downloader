[Setup]
AppName=Downloader
AppVersion=1.0
DefaultDirName={localappdata}\Programs\Downloader
DefaultGroupName=Downloader
UninstallDisplayIcon={app}\Downloader.exe
PrivilegesRequired=lowest
OutputDir=Output
OutputBaseFilename=Downloader_Setup
Compression=lzma2
SolidCompression=yes

[Files]
Source: "build\Release\Downloader.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\Release\yt-dlp.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\Downloader"; Filename: "{app}\Downloader.exe"
Name: "{userstartup}\Downloader"; Filename: "{app}\Downloader.exe"; Tasks: startup

[Tasks]
Name: "startup"; Description: "Automatically start Downloader when Windows starts"; GroupDescription: "Additional icons:"

[Run]
Filename: "{app}\Downloader.exe"; Description: "Launch Downloader now"; Flags: nowait postinstall skipifsilent
