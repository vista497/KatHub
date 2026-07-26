#pragma once

#include <QMainWindow>
#include <QWebEngineView>
#include <QSystemTrayIcon>
#include <QUrl>
#include <QString>

namespace KatHub {

/// WebEngine-based browser window for KatHub Hand mode.
///
/// A QMainWindow with a central QWebEngineView that loads the KatHub
/// server at http://localhost:8080. Closing the window minimizes it
/// to the system tray instead of exiting the application.
class HandWindow : public QMainWindow
{
    Q_OBJECT

public:
    /// Construct a HandWindow that loads @p initialUrl in its WebEngine view.
    /// @param serverPort  Port displayed in the tray menu info item (default 8080).
    explicit HandWindow(const QUrl &initialUrl, int serverPort = 8080,
                        QWidget *parent = nullptr);
    ~HandWindow() override;

public slots:
    /// Navigate the WebEngine view to the given URL.
    void loadUrl(const QUrl &url);

    /// Execute JavaScript in the WebEngine view.
    void executeJavaScript(const QString &js);

signals:
    /// Emitted when a page finishes loading.
    /// @param ok  true if the load succeeded.
    void onPageLoaded(bool ok);

    /// Emitted when the user requests quit from the tray icon.
    /// The owner (KatHubApp) should stop the server and exit cleanly.
    void quitRequested();

protected:
    /// Override closeEvent to minimize to tray instead of closing.
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    void setupTrayIcon();

    QWebEngineView *webView_ = nullptr;
    QSystemTrayIcon *trayIcon_ = nullptr;
    int serverPort_ = 8080;
};

} // namespace KatHub
