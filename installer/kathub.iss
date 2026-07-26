; ============================================================
; KatHub — Inno Setup Installer Script
; Собирает Setup.exe из installer/staging/ (подготовлено CI)
; ============================================================

#define MyAppName "KatHub"
#define MyAppVersion "0.1.0"
#define MyAppExeName "kathub-backend.exe"
#define MyAppPublisher "KatHub"

; CI-path: всё уже в installer/staging после windeployqt
#define StagingDir "installer\staging"

[Setup]
AppId={{KatHub-App-01}}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL=https://github.com/vista497/kathub
AppSupportURL=https://github.com/vista497/kathub/issues
AppUpdatesURL=https://github.com/vista497/kathub/releases
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
Name: "russian"; MessagesFile: "compiler:Languages\Russian.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon";  Description: "Создать ярлык на рабочем столе";  GroupDescription: "Дополнительно:"
Name: "startup";      Description: "Автозапуск при входе в систему";  GroupDescription: "Дополнительно:"; Flags: unchecked

[Files]
; Главный exe
Source: "{#StagingDir}\{#MyAppExeName}";  DestDir: "{app}"; Flags: ignoreversion

; Все DLL (Qt + MSVC) — скопированы windeployqt и скриптом
Source: "{#StagingDir}\*.dll";  DestDir: "{app}"; Flags: ignoreversion

; QtWebEngineProcess.exe
Source: "{#StagingDir}\QtWebEngineProcess.exe"; DestDir: "{app}"; Flags: ignoreversion

; Ресурсы WebEngine (могут отсутствовать с --no-translations)
Source: "{#StagingDir}\resources\*"; DestDir: "{app}\resources"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist

; Локали WebEngine
Source: "{#StagingDir}\translations\*"; DestDir: "{app}\translations"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist

; Платформенные плагины
Source: "{#StagingDir}\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#StagingDir}\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#StagingDir}\imageformats\*"; DestDir: "{app}\imageformats"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#StagingDir}\iconengines\*"; DestDir: "{app}\iconengines"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#StagingDir}\sqldrivers\*"; DestDir: "{app}\sqldrivers"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#StagingDir}\tls\*"; DestDir: "{app}\tls"; Flags: ignoreversion skipifsourcedoesntexist

; Фронтенд (Vue)
Source: "{#StagingDir}\static\*"; DestDir: "{app}\static"; Flags: ignoreversion recursesubdirs

; Шаблоны промптов
Source: "{#StagingDir}\templates\*"; DestDir: "{app}\templates"; Flags: ignoreversion recursesubdirs skipifsourcedoesntexist

[Icons]
Name: "{autoprograms}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{userstartup}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Parameters: "--server"; Tasks: startup

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Запустить KatHub"; Flags: nowait postinstall skipifsilent
