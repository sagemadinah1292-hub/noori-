#ifndef SourceDir
  #define SourceDir "..\..\dist-choir\Noori-Choir-0.3.1-Windows-x64"
#endif
[Setup]
AppId={{F34706D1-B697-461C-9C81-A56C81A39B77}
AppName=NOORI CHOIR
AppVersion=0.3.1
AppPublisher=Noori
DefaultDirName={commoncf64}\VST3
DisableDirPage=no
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputBaseFilename=NOORI_CHOIR_Setup_x64
Compression=lzma2
SolidCompression=yes
UninstallFilesDir={app}\NOORI CHOIR Uninstall
UninstallDisplayName=NOORI CHOIR
CloseApplications=no
SetupLogging=yes
[Files]
Source: "{#SourceDir}\NOORI CHOIR.vst3\*"; DestDir: "{app}\NOORI CHOIR.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SourceDir}\README.md"; DestDir: "{app}\NOORI CHOIR Uninstall"; Flags: ignoreversion
Source: "{#SourceDir}\LICENSE"; DestDir: "{app}\NOORI CHOIR Uninstall"; Flags: ignoreversion
Source: "{#SourceDir}\licenses\*"; DestDir: "{app}\NOORI CHOIR Uninstall\licenses"; Flags: ignoreversion recursesubdirs createallsubdirs
[Messages]
WelcomeLabel2=This installs the experimental NOORI CHOIR Windows x64 VST3. Close Cubase before continuing.%n%nThe included engine uses DSP musical parts and formant processing. No neural singer models are included. Cubase Pro 15 host acceptance testing remains pending.
