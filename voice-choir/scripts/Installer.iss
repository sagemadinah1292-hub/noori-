#ifndef SourceDir
  #define SourceDir "..\..\dist-choir\Noori-Choir-0.4.2-Windows-x64"
#endif
#ifndef ActivationRequired
 #define ActivationRequired 0
#endif
[Setup]
AppId={{F34706D1-B697-461C-9C81-A56C81A39B77}
AppName=NOORI CHOIR
AppVersion=0.4.2
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
#if ActivationRequired
WelcomeLabel2=This installs NOORI CHOIR for registered friends. Close Cubase before continuing.%n%nAn email-bound key and an internet connection are required. Trial keys last three days from first activation. No payment system is included. The engine uses DSP; no neural singer model is included.
#else
WelcomeLabel2=OWNER DEVELOPMENT BUILD: online activation is not connected or enforced. Do not distribute this installer as the protected friends build.%n%nClose Cubase before continuing. This build is for testing the interface and audio workflow. It uses DSP and includes no neural singer model.
#endif
