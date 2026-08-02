#pragma once

#include <QString>
#include <QMap>

namespace KatHub {

/// Centralized application paths.
///
/// All paths are computed relative to the application data directory.
/// Custom overrides can be stored via setCustomPath() / QSettings.
class AppPaths
{
public:
    AppPaths() = delete;

    /// Initialize with the application data directory.
    static void init(const QString &dataDir);

    /// Base data directory.
    static QString dataDir();

    /// Configuration directory (JSON configs, providers.json, etc.).
    static QString configDir();

    /// Logs directory.
    static QString logsDir();

    /// Plugins directory (DLL-based providers).
    static QString pluginsDir();

    // ── Speech (STT/TTS) paths ─────────────────────────────────

    /// Speech scripts root: backend/resources/speech (dev tree) or
    /// <exe_dir>/resources/speech (installed build).
    static QString speechScriptsDir();

    /// STT scripts dir (stt_streaming_server.py, requirements.txt).
    static QString sttScriptsDir();

    /// TTS scripts dir (TTSNodeNew.py, requirements.txt).
    static QString ttsScriptsDir();

    /// Path to STT streaming server script.
    static QString sttStreamingServerPath();

    /// Path to TTS node server script.
    static QString ttsNodeServerPath();

    /// Python venv dir for STT (created on first run by PythonEnvironment).
    static QString pythonVenvDir();

    /// venv python.exe (STT).
    static QString pythonExePath();

    /// venv pip.exe (STT).
    static QString pipExePath();

    /// STT requirements.txt path.
    static QString sttRequirementsPath();

    /// Python venv dir for TTS.
    static QString ttsVenvDir();

    /// venv python.exe (TTS).
    static QString ttsPythonExePath();

    /// faster-whisper model dir (STT-Base).
    static QString sttModelPath();

    // ── Custom path overrides (persisted in QSettings) ──────

    static QString customPath(const QString &key);
    static void    setCustomPath(const QString &key, const QString &path);
    static void    loadCustomPaths();
    static void    saveCustomPaths();

    static constexpr const char *KEY_DATA_DIR   = "paths/data";
    static constexpr const char *KEY_CONFIG_DIR  = "paths/config";
    static constexpr const char *KEY_LOGS_DIR    = "paths/logs";
    static constexpr const char *KEY_PLUGINS_DIR = "paths/plugins";

private:
    static QString resolve(const QString &customKey,
                           const QString &defaultPath);

    static QString                  s_dataDir;
    static QMap<QString, QString>   s_customPaths;
};

} // namespace KatHub
