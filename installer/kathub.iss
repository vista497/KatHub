; ============================================================
; KatHub — Inno Setup Installer Script (Phase 6)
; Генерирует Setup.exe с полным окружением Qt 6.7.3
; ============================================================

#define MyAppName "KatHub"
#define MyAppVersion "0.1.0"
#define MyAppExeName "kathub-backend.exe"
#define MyAppPublisher "KatHub"

; ── Qt 6.7.3 paths ──────────────────────────────────────────
#define QtDir "C:\Qt_new\6.7.3\msvc2022_64"
#define QtBin QtDir + "\bin"
#define QtPlugins QtDir + "\plugins"
#define QtResources QtDir + "\resources"
#define QtTranslations QtDir + "\translations"

; ── MSVC 2022 runtime paths ─────────────────────────────────
#define VcRedist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Redist\MSVC\14.44.35112\x64\Microsoft.VC143.CRT"

; ── KatHub paths ────────────────────────────────────────────
#define KatHubRoot "D:\vs\Project421\1_projectGPT\1_Main\KatHub"
#define KatHubBuild KatHubRoot + "\build2\backend\Debug"
#define KatHubStatic KatHubRoot + "\backend\static"

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
OutputDir=..\publish
OutputBaseFilename=KatHub_Setup_{#MyAppVersion}
; SetupIconFile — нет своей иконки; используем стандартную Inno Setup
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
; ── Главный исполняемый файл ─────────────────────────────────
Source: "{#KatHubBuild}\{#MyAppExeName}";  DestDir: "{app}"; Flags: ignoreversion

; ── Qt 6.7.3 DLLs (только release, без debug-суффикса 'd') ──
Source: "{#QtBin}\Qt6Core.dll";              DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtBin}\Qt6Gui.dll";               DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtBin}\Qt6Widgets.dll";           DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtBin}\Qt6Network.dll";           DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtBin}\Qt6WebSockets.dll";        DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtBin}\Qt6WebChannel.dll";        DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtBin}\Qt6WebEngineCore.dll";     DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtBin}\Qt6WebEngineWidgets.dll";  DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtBin}\Qt6Positioning.dll";       DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtBin}\Qt6Quick.dll";             DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtBin}\Qt6QuickWidgets.dll";      DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtBin}\Qt6Qml.dll";               DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtBin}\Qt6QmlCore.dll";           DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtBin}\Qt6QmlModels.dll";         DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtBin}\Qt6QmlLocalStorage.dll";   DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtBin}\Qt6QmlWorkerScript.dll";   DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtBin}\Qt6QmlNetwork.dll";        DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtBin}\Qt6QmlXmlListModel.dll";   DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtBin}\Qt6OpenGL.dll";            DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtBin}\Qt6OpenGLWidgets.dll";     DestDir: "{app}"; Flags: ignoreversion
Source: "{#QtBin}\Qt6DBus.dll";              DestDir: "{app}"; Flags: ignoreversion

; ── QtWebEngineProcess.exe (Chromium helper, нужен для Hand-режима) ──
Source: "{#QtBin}\QtWebEngineProcess.exe";   DestDir: "{app}"; Flags: ignoreversion

; ── Qt Plugins ────────────────────────────────────────────────
Source: "{#QtPlugins}\platforms\qwindows.dll";      DestDir: "{app}\platforms";       Flags: ignoreversion
Source: "{#QtPlugins}\platforms\qminimal.dll";      DestDir: "{app}\platforms";       Flags: ignoreversion
Source: "{#QtPlugins}\styles\qmodernwindowsstyle.dll"; DestDir: "{app}\styles"; Flags: ignoreversion
Source: "{#QtPlugins}\imageformats\qgif.dll";       DestDir: "{app}\imageformats";    Flags: ignoreversion
Source: "{#QtPlugins}\imageformats\qico.dll";       DestDir: "{app}\imageformats";    Flags: ignoreversion
Source: "{#QtPlugins}\imageformats\qjpeg.dll";      DestDir: "{app}\imageformats";    Flags: ignoreversion
Source: "{#QtPlugins}\imageformats\qsvg.dll";       DestDir: "{app}\imageformats";    Flags: ignoreversion
Source: "{#QtPlugins}\iconengines\qsvgicon.dll";    DestDir: "{app}\iconengines";    Flags: ignoreversion
Source: "{#QtPlugins}\sqldrivers\qsqlite.dll";      DestDir: "{app}\sqldrivers";     Flags: ignoreversion
Source: "{#QtPlugins}\tls\qschannelbackend.dll";     DestDir: "{app}\tls";             Flags: ignoreversion
Source: "{#QtPlugins}\tls\qcertonlybackend.dll";     DestDir: "{app}\tls";             Flags: ignoreversion

; ── Qt WebEngine resources (.pak файлы Chromium) ─────────────
Source: "{#QtResources}\qtwebengine_resources.pak";           DestDir: "{app}\resources"; Flags: ignoreversion
Source: "{#QtResources}\qtwebengine_resources_100p.pak";      DestDir: "{app}\resources"; Flags: ignoreversion
Source: "{#QtResources}\qtwebengine_resources_200p.pak";      DestDir: "{app}\resources"; Flags: ignoreversion
Source: "{#QtResources}\qtwebengine_devtools_resources.pak";  DestDir: "{app}\resources"; Flags: ignoreversion
Source: "{#QtResources}\icudtl.dat";                          DestDir: "{app}\resources"; Flags: ignoreversion

; ── Qt WebEngine locales (первые 10 самых нужных + en/ru) ───
Source: "{#QtTranslations}\qtwebengine_locales\en-US.pak";  DestDir: "{app}\translations\qtwebengine_locales"; Flags: ignoreversion
Source: "{#QtTranslations}\qtwebengine_locales\ru.pak";     DestDir: "{app}\translations\qtwebengine_locales"; Flags: ignoreversion
Source: "{#QtTranslations}\qtwebengine_locales\en.pak";     DestDir: "{app}\translations\qtwebengine_locales"; Flags: ignoreversion

; ── MSVC 2022 runtime DLLs (x64) ─────────────────────────────
Source: "{#VcRedist}\msvcp140.dll";               DestDir: "{app}"; Flags: ignoreversion
Source: "{#VcRedist}\msvcp140_1.dll";             DestDir: "{app}"; Flags: ignoreversion
Source: "{#VcRedist}\msvcp140_2.dll";             DestDir: "{app}"; Flags: ignoreversion
Source: "{#VcRedist}\msvcp140_atomic_wait.dll";   DestDir: "{app}"; Flags: ignoreversion
Source: "{#VcRedist}\msvcp140_codecvt_ids.dll";   DestDir: "{app}"; Flags: ignoreversion
Source: "{#VcRedist}\vcruntime140.dll";           DestDir: "{app}"; Flags: ignoreversion
Source: "{#VcRedist}\vcruntime140_1.dll";         DestDir: "{app}"; Flags: ignoreversion
Source: "{#VcRedist}\vcruntime140_threads.dll";   DestDir: "{app}"; Flags: ignoreversion
Source: "{#VcRedist}\vccorlib140.dll";            DestDir: "{app}"; Flags: ignoreversion
Source: "{#VcRedist}\concrt140.dll";              DestDir: "{app}"; Flags: ignoreversion

; ── Frontend static (собранный Vue 3) ────────────────────────
Source: "{#KatHubStatic}\index.html";        DestDir: "{app}\static";            Flags: ignoreversion
Source: "{#KatHubStatic}\favicon.svg";       DestDir: "{app}\static";            Flags: ignoreversion
Source: "{#KatHubStatic}\icons.svg";         DestDir: "{app}\static";            Flags: ignoreversion
Source: "{#KatHubStatic}\assets\*";          DestDir: "{app}\static\assets";     Flags: ignoreversion

; ── Конфигурация (providers.json — копируется если есть) ─────
Source: "{#KatHubRoot}\providers.json";  DestDir: "{app}\config"; Flags: ignoreversion onlyifdoesntexist

[Icons]
Name: "{group}\{#MyAppName}";                       Filename: "{app}\{#MyAppExeName}"; Parameters: "--server"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}";                 Filename: "{app}\{#MyAppExeName}"; Parameters: "--server"; Tasks: desktopicon

[Registry]
; Автозапуск (опционально) — запуск в режиме сервера при входе в Windows
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueType: string; ValueName: "KatHub"; ValueData: """{app}\{#MyAppExeName}"" --server"; Flags: uninsdeletevalue; Tasks: startup

; Сохраняем путь установки для будущих обновлений
Root: HKCU; Subkey: "Software\KatHub"; ValueType: string; ValueName: "InstallPath"; ValueData: "{app}"; Flags: uninsdeletekey
Root: HKCU; Subkey: "Software\KatHub"; ValueType: string; ValueName: "Version"; ValueData: "{#MyAppVersion}"

[Run]
; Запуск KatHub после установки
Filename: "{app}\{#MyAppExeName}"; Parameters: "--server"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
  begin
    ; Сохраняем путь установки и версию в реестр для автообновления
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
