; TOPVOX AI STUDIO - script d'installation (Inno Setup)
[Setup]
AppId={{B6F1E5D2-7A34-4C1B-9E52-3D8F0A7C1B44}
AppName=TOPVOX AI STUDIO
AppVersion=1.0.0
AppPublisher=TOPVOX
DefaultDirName={autopf}\TOPVOX AI STUDIO
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=TOPVOX_AI_SETUP
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayIcon={app}\TOPVOX_AI.exe

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"

[Files]
Source: "..\artifacts\TOPVOX_AI.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\artifacts\TOPVOX_AI.vst3\*"; DestDir: "{commoncf64}\VST3\TOPVOX_AI.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autodesktop}\TOPVOX AI STUDIO"; Filename: "{app}\TOPVOX_AI.exe"
Name: "{autoprograms}\TOPVOX AI STUDIO\TOPVOX AI STUDIO"; Filename: "{app}\TOPVOX_AI.exe"
Name: "{autoprograms}\TOPVOX AI STUDIO\Desinstaller TOPVOX AI STUDIO"; Filename: "{uninstallexe}"

[Run]
Filename: "{app}\TOPVOX_AI.exe"; Description: "Lancer TOPVOX AI STUDIO"; Flags: nowait postinstall skipifsilent
