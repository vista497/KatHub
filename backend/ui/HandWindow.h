#pragma once

#include <QMainWindow>
#include <QWebEngineView>
#include <QUrl>
#include <QString>

namespace KatHub {

/// WebEngine-based browser window for KatHub Hand mode.
///
/// A frameless QMainWindow with a custom title bar and a central
/// QWebEngineView. Receives navigation and JavaScript-execution
/// commands from SignalHub events (navigate.to / webengine.executeJs).
class HandWindow : public QMainWindow
{
    Q_OBJECT

public:
    /// Construct a HandWindow that loads @p initialUrl in its WebEngine view.
    explicit HandWindow(const QUrl &initialUrl, QWidget *parent = nullptr);
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

private:
    void setupUi();
    QWidget *createTitleBar();

    QWebEngineView *webView_ = nullptr;
    QWidget *titleBar_ = nullptr;
};

} // namespace KatHub
