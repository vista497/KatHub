#include "HandWindow.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace KatHub {

// ---------------------------------------------------------------------------
//  Construction / Destruction
// ---------------------------------------------------------------------------

HandWindow::HandWindow(const QUrl &initialUrl, QWidget *parent)
    : QMainWindow(parent)
{
    // Frameless window so we can draw our own title bar.
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);

    setupUi();

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
    // Central widget: title bar on top, WebEngine view below.
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("centralWidget"));

    auto *vbox = new QVBoxLayout(central);
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    // Custom title bar.
    titleBar_ = createTitleBar();
    vbox->addWidget(titleBar_);

    // WebEngine view fills the remaining space.
    webView_ = new QWebEngineView(central);
    webView_->setObjectName(QStringLiteral("webView"));

    // Forward QWebEngineView::loadFinished to our own signal.
    connect(webView_, &QWebEngineView::loadFinished,
            this, &HandWindow::onPageLoaded);

    vbox->addWidget(webView_, /*stretch=*/1);

    setCentralWidget(central);

    // Apply dark theme to the window.
    setStyleSheet(QStringLiteral(
        "#centralWidget { background-color: #1e1e2e; }"
        "QWebEngineView { border: none; }"
    ));

    resize(1280, 800);
    setWindowTitle(QStringLiteral("KatHub"));
}

QWidget *HandWindow::createTitleBar()
{
    constexpr int kTitleBarHeight = 40;
    const QString accentColor = QStringLiteral("#6C5CE7");
    const QString bgColor = QStringLiteral("#2d2d3f");
    const QString textColor = QStringLiteral("#cdd6f4");

    auto *bar = new QWidget();
    bar->setObjectName(QStringLiteral("titleBar"));
    bar->setFixedHeight(kTitleBarHeight);
    bar->setStyleSheet(
        QStringLiteral("#titleBar { background-color: %1; }").arg(bgColor));

    auto *hbox = new QHBoxLayout(bar);
    hbox->setContentsMargins(12, 0, 8, 0);
    hbox->setSpacing(0);

    // Application name label.
    auto *title = new QLabel(QStringLiteral("KatHub"), bar);
    title->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 14px; font-weight: bold; }")
            .arg(accentColor));
    hbox->addWidget(title);

    hbox->addStretch();

    // Close button.
    auto *closeBtn = new QPushButton(QStringLiteral("\u2715"), bar);  // ✕
    closeBtn->setFixedSize(32, 28);
    closeBtn->setCursor(Qt::PointingHandCursor);
    closeBtn->setStyleSheet(QStringLiteral(
        "QPushButton {"
        "  background: transparent;"
        "  color: %1;"
        "  border: none;"
        "  border-radius: 6px;"
        "  font-size: 16px;"
        "}"
        "QPushButton:hover {"
        "  background-color: %2;"
        "  color: #ffffff;"
        "}"
    ).arg(textColor, accentColor));
    connect(closeBtn, &QPushButton::clicked, this, &QMainWindow::close);
    hbox->addWidget(closeBtn);

    return bar;
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
