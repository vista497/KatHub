; ============================================================
; KatHub -- Inno Setup Installer Script
; Consumes pre-built staging/ directory from package_staging.py.
; ============================================================

#define MyAppName "KatHub"
#define MyAppVersion "0.6.0"
#define MyAppExeName "kathub-backend.exe"
#define MyAppPublisher "KatHub"

; -- Staging directory (relative to this .iss file) -------------
#define StagingDir "staging"

[Setup]
AppId={{KatHub-App-02}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL=https://github.com/nousresearch/kathub
AppSupportURL=https://github.com/nousresearch/kathub/issues
AppUpdatesURL=https://github.com/nousresearch/kathub/releases
DefaultDirName={localappdata}\KatHub
DefaultGroupName=KatHub
DisableProgramGroupPage=yes
OutputDir=..\publish
OutputBaseFilename=KatHub_Setup_{#MyAppVersion}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
PrivilegesRequired=lowest
UninstallDisplayName=KatHub
UninstallDisplayIcon={app}\{#MyAppExeName}
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
DisableDirPage=no
AlwaysShowDirOnReadyPage=yes

[Languages]
Name: "english";   MessagesFile: "compiler:Default.isl"
Name: "russian";   MessagesFile: "compiler:Languages\Russian.isl"

[Tasks]
Name: "desktopicon";  Description: "Create a &desktop shortcut";  GroupDescription: "Additional:"
Name: "startup";      Description: "Auto-start on login (--server mode)";  GroupDescription: "Additional:"; Flags: unchecked

[Files]
; -- Everything from staging (flat copy, preserves subdirs) -----
Source: "{#StagingDir}\*";  DestDir: "{app}";  Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}";                       Filename: "{app}\{#MyAppExeName}"; Parameters: "--server"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}";                 Filename: "{app}\{#MyAppExeName}"; Parameters: "--server"; Tasks: desktopicon

[Registry]
; Auto-start (optional) -- server mode on Windows login
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "KatHub"; ValueData: """{app}\{#MyAppExeName}"" --server"; Flags: uninsdeletevalue; Tasks: startup

; Persist install path + version for future updates
Root: HKCU; Subkey: "Software\KatHub"; ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\KatHub"; ValueType: string; ValueName: "Version"; ValueData: "{#MyAppVersion}"

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    RegWriteStringValue(
      HKCU, 'Software\KatHub',
      'InstallPath', ExpandConstant('{app}')
    );
    RegWriteStringValue(
      HKCU, 'Software\KatHub',
      'Version', '{#MyAppVersion}'
    );
  end;
end;
