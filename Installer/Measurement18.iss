#define AppName "Physical Model Guitar Measurement Station"
#define AppVersion "1.8 Research"
#ifndef RepoRoot
#define RepoRoot "."
#endif
#ifndef OutputRoot
#define OutputRoot "."
#endif
[Setup]
AppId={{CEE2B6F0-8139-48C1-86FE-1D9B37094D2B}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Physical Model Guitar Research
DefaultDirName={autopf}\PhysicalModelGuitar\MeasurementLab18
DefaultGroupName=Physical Model Guitar Research
DisableProgramGroupPage=yes
OutputDir={#OutputRoot}
OutputBaseFilename=PhysicalModelGuitar-1.8-Measurement-Lab-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
UninstallDisplayName=Physical Model Guitar Measurement Station 1.8 Research
WizardStyle=modern
[Files]
Source: "{#RepoRoot}\dist\PMGMeasurement18.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RepoRoot}\dist\PMGQualify18.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RepoRoot}\dist\PMGChannelLatency18.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#RepoRoot}\README_18_JA.md"; DestDir: "{app}"; Flags: ignoreversion
[Icons]
Name: "{autoprograms}\Physical Model Guitar 1.8 Measurement Terminal"; Filename: "{cmd}"; Parameters: "/K cd /d ""{app}"""; WorkingDir: "{app}"
Name: "{autoprograms}\Uninstall PMG Measurement Lab"; Filename: "{uninstallexe}"
