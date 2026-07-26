#include "SetupWizard.h"

#include <QApplication>
#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QTextStream>
#include <QVBoxLayout>

namespace KatHub {

// ── Marker file path ────────────────────────────────────────────────
static QString markerPath()
{
    return QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath("katHubSetupDone");
}

bool SetupWizard::isAlreadyConfigured()
{
    return QFile::exists(markerPath());
}

void SetupWizard::markConfigured()
{
    QDir().mkpath(QFileInfo(markerPath()).absolutePath());
    QFile f(markerPath());
    f.open(QIODevice::WriteOnly | QIODevice::Truncate);
    f.close();
}

// ── Hermes detection ────────────────────────────────────────────────
static bool detectHermesInstall(QString &foundPath)
{
    // Check %LOCALAPPDATA%\hermes\hermes.exe first
    QString localAppData = QStandardPaths::writableLocation(
        QStandardPaths::GenericCacheLocation);
    // GenericCacheLocation → .../AppData/Local/cache; go two up
    QDir d(localAppData);
    d.cdUp();
    d.cdUp(); // now in .../AppData/Local

    QStringList candidates = {
        d.absoluteFilePath("hermes/hermes.exe"),
        d.absoluteFilePath("Programs/hermes/bin/hermes.exe"),
    };

    // Also check PATH for 'hermes'
    for (const auto &c : candidates) {
        if (QFileInfo::exists(c)) {
            foundPath = QDir::toNativeSeparators(c);
            return true;
        }
    }

    // Try 'where hermes'
    QProcess proc;
    proc.start("where", QStringList() << "hermes");
    proc.waitForFinished(3000);
    if (proc.exitCode() == 0) {
        QString out = proc.readAllStandardOutput().trimmed();
        if (!out.isEmpty()) {
            foundPath = out.split('\n').first().trimmed();
            return true;
        }
    }

    return false;
}

static bool checkApiKey(const QString &key)
{
    // API key starts with "kt-" and is ~48 chars
    return key.startsWith("kt-") && key.length() >= 32;
}

// ── Constructor ─────────────────────────────────────────────────────
SetupWizard::SetupWizard(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QString::fromUtf8("KatHub — первоначальная настройка"));
    setMinimumSize(520, 380);
    setModal(true);
    setupUi();
}

void SetupWizard::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    // ── Page 0: Welcome ─────────────────────────────────────────
    pageWelcome_ = new QWidget;
    {
        auto *lay = new QVBoxLayout(pageWelcome_);
        auto *title = new QLabel(QString::fromUtf8(
            "<h2>Добро пожаловать в KatHub</h2>"));
        title->setWordWrap(true);
        lay->addWidget(title);

        auto *desc = new QLabel(QString::fromUtf8(
            "<p>Похоже, это первый запуск. Давай настроим всё, "
            "что нужно для работы:</p>"
            "<ul>"
            "<li><b>Hermes Agent</b> — AI-движок (должен быть установлен)</li>"
            "<li><b>API-ключ</b> — связь KatHub с Hermes</li>"
            "</ul>"
            "<p>Это займёт пару минут.</p>"));
        desc->setWordWrap(true);
        lay->addWidget(desc);
        lay->addStretch();
    }
    mainLayout->addWidget(pageWelcome_);

    // ── Page 1: Hermes check ────────────────────────────────────
    pageHermes_ = new QWidget;
    {
        auto *lay = new QVBoxLayout(pageHermes_);
        auto *title = new QLabel(QString::fromUtf8(
            "<h3>Шаг 1: Hermes Agent</h3>"));
        title->setWordWrap(true);
        lay->addWidget(title);

        hermesStatus_ = new QLabel(QString::fromUtf8(
            "Проверяю наличие Hermes на компьютере..."));
        hermesStatus_->setWordWrap(true);
        lay->addWidget(hermesStatus_);

        auto *btnCheck = new QPushButton(QString::fromUtf8("Проверить ещё раз"));
        connect(btnCheck, &QPushButton::clicked,
                this, &SetupWizard::checkHermesNow);
        lay->addWidget(btnCheck);

        auto *btnDownload = new QPushButton(QString::fromUtf8(
            "Открыть страницу загрузки Hermes"));
        btnDownload->setStyleSheet("QPushButton { color: #2196F3; }");
        connect(btnDownload, &QPushButton::clicked,
                this, &SetupWizard::openDownloadPage);
        lay->addWidget(btnDownload);

        lay->addStretch();
    }
    mainLayout->addWidget(pageHermes_);

    // ── Page 2: API key ─────────────────────────────────────────
    pageApiKey_ = new QWidget;
    {
        auto *lay = new QVBoxLayout(pageApiKey_);
        auto *title = new QLabel(QString::fromUtf8(
            "<h3>Шаг 2: API-ключ Hermes</h3>"));
        title->setWordWrap(true);
        lay->addWidget(title);

        auto *desc = new QLabel(QString::fromUtf8(
            "<p>Найди свой ключ в Hermes Dashboard или в файле "
            "<code>AppData/Local/hermes/.env</code> — строка "
            "<b>API_SERVER_KEY</b>.</p>"
            "<p>Выглядит как <code>kt-c3274...</code></p>"));
        desc->setWordWrap(true);
        lay->addWidget(desc);

        auto *form = new QFormLayout;
        apiKeyField_ = new QLineEdit;
        apiKeyField_->setPlaceholderText("kt-...");
        apiKeyField_->setEchoMode(QLineEdit::Password);
        form->addRow(QString::fromUtf8("Ключ:"), apiKeyField_);
        lay->addLayout(form);

        apiKeyError_ = new QLabel;
        apiKeyError_->setStyleSheet("color: red;");
        apiKeyError_->setVisible(false);
        lay->addWidget(apiKeyError_);

        auto *btnCheckKey = new QPushButton(
            QString::fromUtf8("Проверить ключ"));
        connect(btnCheckKey, &QPushButton::clicked,
                this, &SetupWizard::validateApiKey);
        lay->addWidget(btnCheckKey);

        lay->addStretch();
    }
    mainLayout->addWidget(pageHermes_);
    mainLayout->addWidget(pageApiKey_);

    // ── Page 3: Finish ──────────────────────────────────────────
    pageFinish_ = new QWidget;
    {
        auto *lay = new QVBoxLayout(pageFinish_);
        auto *title = new QLabel(QString::fromUtf8(
            "<h2>Готово!</h2>"));
        title->setWordWrap(true);
        lay->addWidget(title);

        auto *desc = new QLabel(QString::fromUtf8(
            "<p>Всё настроено. Сейчас откроется главное окно KatHub.</p>"
            "<p>Если позже понадобится изменить настройки — "
            "удали файл <code>katHubSetupDone</code> в папке приложения.</p>"));
        desc->setWordWrap(true);
        lay->addWidget(desc);
        lay->addStretch();
    }
    mainLayout->addWidget(pageFinish_);

    // ── Bottom buttons ──────────────────────────────────────────
    auto *btnRow = new QHBoxLayout;

    btnSkip_ = new QPushButton(QString::fromUtf8("Пропустить"));
    connect(btnSkip_, &QPushButton::clicked, this, [this]() {
        // On last page, "Skip" means: save what we have and finish
        finish();
    });
    btnRow->addWidget(btnSkip_);

    btnNext_ = new QPushButton(QString::fromUtf8("Далее"));
    btnNext_->setDefault(true);
    connect(btnNext_, &QPushButton::clicked, this, [this]() {
        if (currentPage_ < 3) {
            // Show next page
            currentPage_++;
        }
        if (currentPage_ == 3) {
            // On finish page — swap buttons
            btnNext_->setVisible(false);
            btnSkip_->setText(QString::fromUtf8("Завершить"));
            btnFinish_ = btnSkip_;
        }
        // Update visibility
        pageWelcome_->setVisible(currentPage_ == 0);
        pageHermes_->setVisible(currentPage_ == 1);
        pageApiKey_->setVisible(currentPage_ == 2);
        pageFinish_->setVisible(currentPage_ == 3);

        if (currentPage_ == 1) checkHermesNow();
    });
    btnRow->addWidget(btnNext_);
    btnRow->addStretch();

    mainLayout->addLayout(btnRow);

    // ── Initial state ───────────────────────────────────────────
    currentPage_ = 0;
    pageWelcome_->setVisible(true);
    pageHermes_->setVisible(false);
    pageApiKey_->setVisible(false);
    pageFinish_->setVisible(false);

    // Check Hermes in advance
    checkHermesNow();
}

void SetupWizard::checkHermesNow()
{
    QString foundPath;
    hermesFound_ = detectHermesInstall(foundPath);

    if (hermesFound_) {
        hermesStatus_->setText(QString::fromUtf8(
            "<b style='color:#4CAF50'>✓ Hermes найден:</b><br>%1<br>"
            "<small>Отлично, всё в порядке.</small>")
            .arg(foundPath));
    } else {
        hermesStatus_->setText(QString::fromUtf8(
            "<b style='color:#F44336'>✗ Hermes не найден</b><br>"
            "<small>Hermes Agent нужен для работы KatHub. "
            "Нажми кнопку ниже, чтобы скачать установщик Hermes, "
            "или укажи путь вручную.</small>"));
    }
}

void SetupWizard::openDownloadPage()
{
    QDesktopServices::openUrl(
        QUrl("https://hermes-agent.nousresearch.com/docs"));
}

void SetupWizard::validateApiKey()
{
    QString key = apiKeyField_->text().trimmed();
    if (key.isEmpty()) {
        apiKeyError_->setText(QString::fromUtf8(
            "Введи API-ключ или нажми «Пропустить»."));
        apiKeyError_->setVisible(true);
        return;
    }
    if (!checkApiKey(key)) {
        apiKeyError_->setText(QString::fromUtf8(
            "Ключ должен начинаться с «kt-» и быть не менее 32 символов."));
        apiKeyError_->setVisible(true);
        return;
    }
    // Basic validation passed
    apiKeyField_->setText(key); // trimmed
    apiKeyError_->setVisible(false);

    // Write .env
    QString envPath = QDir(QCoreApplication::applicationDirPath())
        .absoluteFilePath(".env");
    QDir().mkpath(QFileInfo(envPath).absolutePath());
    QFile f(envPath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QTextStream out(&f);
        out << "API_SERVER_KEY=" << key << "\n";
        f.close();
        apiKey_ = key;
        apiKeyError_->setStyleSheet("color: #4CAF50;");
        apiKeyError_->setText(QString::fromUtf8(
            "✓ Ключ сохранён в %1").arg(QDir::toNativeSeparators(envPath)));
        apiKeyError_->setVisible(true);
    } else {
        apiKeyError_->setStyleSheet("color: red;");
        apiKeyError_->setText(QString::fromUtf8(
            "Ошибка: не могу записать %1").arg(envPath));
        apiKeyError_->setVisible(true);
    }
}

void SetupWizard::finish()
{
    // Save whatever was entered
    if (!apiKeyField_->text().trimmed().isEmpty()
        && checkApiKey(apiKeyField_->text().trimmed())) {
        validateApiKey();
    }
    ok_ = true;
    markConfigured();
    accept();
}

} // namespace KatHub
