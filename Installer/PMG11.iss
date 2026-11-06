; Physical Model Guitar 1.1 Research - Windows x64 VST3 installer.
; Build with Inno Setup 6 via ISCC.exe /DRepoRoot=<root> /DBuildRoot=<path>
#ifndef RepoRoot
  #define RepoRoot ".."
#endif
#ifndef BuildRoot
  #define BuildRoot "..\build-win"
#endif
#ifndef OutputRoot
  #define OutputRoot "..\dist"
#endif
#define PluginName "Physical Model Guitar 1.1 Research"
#define ProductGuid "{{E116F63A-9901-46AA-BE63-2104FA5A1411}"
[Setup]
AppId={#ProductGuid}
AppName={#PluginName}
AppVersion=1.1.0
AppPublisher=Personal Utility Lab
DefaultDirName={autocf}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
UninstallDisplayName={#PluginName}
OutputDir={#OutputRoot}
OutputBaseFilename=PhysicalModelGuitar-1.1-Research-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
SetupLogging=yes
CloseApplications=yes
RestartApplications=no
[Files]
Source: "{#BuildRoot}\PhysicalModelGuitar_artefacts\Release\VST3\Physical Model Guitar 1.1 Research.vst3\*"; DestDir: "{app}\Physical Model Guitar 1.1 Research.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion
[Icons]
; Plugin instruments do not require a Start Menu shortcut.
