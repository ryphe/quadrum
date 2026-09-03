[Setup]
AppId={{EB04B394-ACF1-405D-B0D3-20DB932FA574}
AppName=quadrum
AppVersion=1.0
AppPublisher=ryphe
DefaultDirName={autopf}\quadrum
UsePreviousAppDir=no
DefaultGroupName=quadrum
SetupIconFile=quadrum.ico
UninstallDisplayIcon={app}\quadrum.ico
Compression=lzma2/ultra64
SolidCompression=yes
OutputDir=.
OutputBaseFilename=quadrum_setup_1.0
WizardStyle=modern
ChangesAssociations=no
DirExistsWarning=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "quadrum.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "quadrum.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\quadrum"; Filename: "{app}\quadrum.exe"; IconFilename: "{app}\quadrum.ico"
Name: "{group}\{cm:UninstallProgram,quadrum}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\quadrum"; Filename: "{app}\quadrum.exe"; IconFilename: "{app}\quadrum.ico"; Tasks: desktopicon

[UninstallDelete]
Type: files; Name: "{app}\quadrum.exe"
Type: files; Name: "{app}\quadrum.ico"
Type: files; Name: "{app}\unins*.exe"
Type: files; Name: "{app}\unins*.dat"

[Run]
Filename: "{app}\quadrum.exe"; Description: "{cm:LaunchProgram,quadrum}"; Flags: nowait postinstall skipifsilent