; ============================================================
; KatHub — Inno Setup Installer Script
; Все файлы собираются из staging/ (windeployqt + MSVC DLLs + frontend)
; Запускать из корня проекта: iscc installer/kathub.iss
; ============================================================

#define MyAppName "KatHub"
#define MyAppVersion "0.1.0"
#define MyAppExeName "kathub-backend.exe"
#define MyAppPublisher "KatHub"

; Все файлы берутся из installer/staging/ — копия собранного приложения
#define StagingDir "staging"

[Setup]
AppId={{KatHub-App-01}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL=https://github.com/nousresearch/kathub
AppSupportURL=https://github.com/nousresearch/kathub/issues
AppUpdatesURL=https://github.com/nousresearch/kathub/releases
DefaultDirName={localappdata}\KatHub
DefaultGroupName=KatHub
DisableProgramGroupPage=yes
OutputDir=publish
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
Name: "desktopicon";  Description: "Создать &ярлык на рабочем столе";  GroupDescription: "Дополнительно:"
Name: "startup";      Description: "Автозапуск при входе в систему (режим --server)";  GroupDescription: "Дополнительно:"; Flags: unchecked

[Files]
; Главный исполняемый файл
Source: "{#StagingDir}\kathub-backend.exe";  DestDir: "{app}"; Flags: ignoreversion

; Все Qt DLLs (уже скопированы windeployqt в staging)
Source: "{#StagingDir}\*.dll";  DestDir: "{app}"; Flags: ignoreversion

; QtWebEngineProcess.exe
Source: "{#StagingDir}\QtWebEngineProcess.exe";  DestDir: "{app}"; Flags: ignoreversion

; Qt Plugins (если есть в staging)
Source: "{#StagingDir}\platforms\*";      DestDir: "{app}\platforms";           Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StagingDir}\styles\*";          DestDir: "{app}\styles";              Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StagingDir}\imageformats\*";    DestDir: "{app}\imageformats";        Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StagingDir}\iconengines\*";     DestDir: "{app}\iconengines";         Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StagingDir}\sqldrivers\*";      DestDir: "{app}\sqldrivers";          Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#StagingDir}\tls\*";             DestDir: "{app}\tls";                 Flags: ignoreversion recursesubdirs createallsubdirs

; Qt WebEngine resources
Source: "{#StagingDir}\resources\*";       DestDir: "{app}\resources";           Flags: ignoreversion recursesubdirs createallsubdirs

; Qt WebEngine locales
Source: "{#StagingDir}\translations\*";    DestDir: "{app}\translations";        Flags: ignoreversion recursesubdirs createallsubdirs

; Frontend static
Source: "{#StagingDir}\static\*";          DestDir: "{app}\static";              Flags: ignoreversion recursesubdirs createallsubdirs

; Prompt templates
Source: "{#StagingDir}\templates\*";       DestDir: "{app}\templates";           Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#MyAppName}";                       Filename: "{app}\{#MyAppExeName}"; Parameters: "--server"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}";                 Filename: "{app}\{#MyAppExeName}"; Parameters: "--server"; Tasks: desktopicon

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "KatHub"; ValueData: """{app}\{#MyAppExeName}"" --server"; Flags: uninsdeletevalue; Tasks: startup
Root: HKCU; Subkey: "Software\KatHub"; ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\KatHub"; ValueType: string; ValueName: "Version"; ValueData: "{#MyAppVersion}"

[Run]
Filename: "{app}\{#MyAppExeName}"; Parameters: "--server"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    RegWriteStringValue(HKCU, 'Software\KatHub', 'InstallPath', ExpandConstant('{app}'));
    RegWriteStringValue(HKCU, 'Software\KatHub', 'Version', '{#MyAppVersion}');
  end;
end;
