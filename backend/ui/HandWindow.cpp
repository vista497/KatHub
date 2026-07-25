#include "HandWindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QMenu>
#include <QPainter>
#include <QPixmap>
#include <QVBoxLayout>

namespace KatHub {

// ---------------------------------------------------------------------------
//  Construction / Destruction
// ---------------------------------------------------------------------------

HandWindow::HandWindow(const QUrl &initialUrl, int serverPort, QWidget *parent)
    : QMainWindow(parent), serverPort_(serverPort)
{
    setupUi();
    setupTrayIcon();

    // Load the initial URL once the UI is set up.
    if (!initialUrl.isEmpty()) {
        loadUrl(initialUrl);
    }
}

HandWindow::~HandWindow() = default;

// ---------------------------------------------------------------------------
//  UI setup
// ---------------------------------------------------------------------------

void HandWindow::setupUi()
{
    // Central widget with a WebEngine view that fills the entire window.
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("centralWidget"));

    auto *vbox = new QVBoxLayout(central);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    webView_ = new QWebEngineView(central);
    webView_->setObjectName(QStringLiteral("webView"));

    // Forward QWebEngineView::loadFinished to our own signal.
    connect(webView_, &QWebEngineView::loadFinished,
            this, &HandWindow::onPageLoaded);

    vbox->addWidget(webView_);

    setCentralWidget(central);

    // Default size and title as specified.
    resize(1024, 768);
    setWindowTitle(QStringLiteral("KatHub"));
}

// ---------------------------------------------------------------------------
//  System tray icon
// ---------------------------------------------------------------------------

void HandWindow::setupTrayIcon()
{
    trayIcon_ = new QSystemTrayIcon(this);

    // Generate a 16×16 blue icon with a white "K".
    QPixmap pix(16, 16);
    pix.fill(Qt::blue);
    {
        QPainter p(&pix);
        p.setPen(Qt::white);
        QFont f = p.font();
        f.setPixelSize(12);
        f.setBold(true);
        p.setFont(f);
        p.drawText(QRect(0, 0, 16, 16), Qt::AlignCenter, QStringLiteral("K"));
    }
    trayIcon_->setIcon(QIcon(pix));
    trayIcon_->setToolTip(QStringLiteral("KatHub"));

    // Right-click context menu.
    auto *trayMenu = new QMenu(this);

    auto *showAction = trayMenu->addAction(QStringLiteral("Show KatHub"));
    connect(showAction, &QAction::triggered, this, [this]() {
        show();
        activateWindow();
    });

    // Informational item — server port (disabled, not clickable).
    auto *serverInfo = trayMenu->addAction(
        QStringLiteral("Server: Running on :%1").arg(serverPort_));
    serverInfo->setEnabled(false);

    trayMenu->addSeparator();

    auto *quitAction = trayMenu->addAction(QStringLiteral("Quit"));
    connect(quitAction, &QAction::triggered, this, &HandWindow::quitRequested);

    trayIcon_->setContextMenu(trayMenu);

    // Double-click on tray icon restores the window.
    connect(trayIcon_, &QSystemTrayIcon::activated,
            [this](QSystemTrayIcon::ActivationReason reason) {
                if (reason == QSystemTrayIcon::DoubleClick) {
                    show();
                    activateWindow();
                }
            });

    trayIcon_->show();
}

// ---------------------------------------------------------------------------
//  closeEvent — minimize to tray
// ---------------------------------------------------------------------------

void HandWindow::closeEvent(QCloseEvent *event)
{
    // Minimize to system tray instead of closing.
    hide();
    trayIcon_->showMessage(
        QStringLiteral("KatHub"),
        QStringLiteral("KatHub is still running in the system tray."),
        QSystemTrayIcon::Information,
        3000);
    event->ignore();
}

// ---------------------------------------------------------------------------
//  Slots
// ---------------------------------------------------------------------------

void HandWindow::loadUrl(const QUrl &url)
{
    if (webView_) {
        webView_->load(url);
    }
}

void HandWindow::executeJavaScript(const QString &js)
{
    if (webView_) {
        webView_->page()->runJavaScript(js);
    }
}

} // namespace KatHub
