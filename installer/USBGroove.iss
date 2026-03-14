; USB Groove - Inno Setup Script
; Build: iscc /DAppVersion=1.50 installer\USBGroove.iss

#ifndef AppVersion
  #define AppVersion "0.0"
#endif

[Setup]
AppId={{E8A3B2F1-5C7D-4A9E-B6F0-1D2E3F4A5B6C}
AppName=USBGroove
AppVersion={#AppVersion}
AppVerName=USBGroove v{#AppVersion}
AppPublisher=michaelnoergaard
AppPublisherURL=https://github.com/michaelnoergaard/USB-Groove
AppSupportURL=https://github.com/michaelnoergaard/USB-Groove
AppUpdatesURL=https://github.com/michaelnoergaard/USB-Groove
DefaultDirName={autopf}\USBGroove
DefaultGroupName=USBGroove
LicenseFile=..\LICENSE
OutputDir=..
OutputBaseFilename=USBGroove_Setup_v{#AppVersion}
SetupIconFile=..\icons\app.ico
UninstallDisplayIcon={app}\USBGroove.exe
Compression=lzma
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\USBGroove.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\USBGroove"; Filename: "{app}\USBGroove.exe"
Name: "{group}\{cm:UninstallProgram,USBGroove}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\USBGroove"; Filename: "{app}\USBGroove.exe"; Tasks: desktopicon
