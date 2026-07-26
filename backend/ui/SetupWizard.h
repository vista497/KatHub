#pragma once

#include <QDialog>
#include <QString>

class QLineEdit;
class QLabel;
class QPushButton;

namespace KatHub {

/// First-run setup wizard.
///
/// Shown when KatHub starts for the first time (Hand mode)
/// to guide the user through:
///  1. Detecting / installing Hermes Agent
///  2. Configuring the API key (.env)
class SetupWizard : public QDialog
{
    Q_OBJECT

public:
    explicit SetupWizard(QWidget *parent = nullptr);

    /// Has the setup been completed before? (checks marker file)
    static bool isAlreadyConfigured();

    /// Mark setup as done (writes marker file).
    static void markConfigured();

    /// The API key the user entered (may be empty if skipped).
    QString apiKey() const { return apiKey_; }

    /// Has Hermes been confirmed/configured?
    bool ok() const { return ok_; }

private slots:
    void checkHermesNow();
    void openDownloadPage();
    void validateApiKey();
    void finish();

private:
    void setupUi();

    // ── Pages ──
    QWidget *pageWelcome_  = nullptr;
    QWidget *pageHermes_   = nullptr;
    QWidget *pageApiKey_   = nullptr;
    QWidget *pageFinish_   = nullptr;

    QPushButton *btnNext_     = nullptr;
    QPushButton *btnSkip_     = nullptr;
    QPushButton *btnFinish_   = nullptr;

    QLabel *hermesStatus_     = nullptr;
    QLabel *apiKeyError_      = nullptr;
    QLineEdit *apiKeyField_   = nullptr;

    int currentPage_ = 0;
    bool hermesFound_ = false;
    QString savedKey_;
    QString apiKey_;
    bool ok_ = false;
};

} // namespace KatHub
