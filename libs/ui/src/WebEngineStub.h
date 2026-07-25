#pragma once

#include <QWidget>
#include <QUrl>
#include <QString>

// Forward declare — QWebEngineView is a heavy include,
// we keep it out of the header via PIMPL where possible,
// but for a stub this is fine.
class QWebEngineView;

namespace KatHub {

/// Thin stub over QWebEngineView for future AI-driven browser automation.
///
/// Currently provides:
///   - loadUrl(QUrl)   — navigate to a page
///   - executeJavaScript(QString) — run JS in the current page
///   - onPageLoaded(bool) signal — emitted when a page finishes loading
///
/// This widget embeds a QWebEngineView directly. In later phases,
/// automation hooks (DOM inspection, screenshot capture, etc.) will be added.
class WebEngineStub : public QWidget
{
    Q_OBJECT

public:
    explicit WebEngineStub(QWidget *parent = nullptr);
    ~WebEngineStub() override;

    /// Navigate the embedded view to @p url.
    void loadUrl(const QUrl &url);

    /// Execute raw JavaScript in the current page context.
    /// @param js  JavaScript source code. The result is ignored in this stub.
    void executeJavaScript(const QString &js);

signals:
    /// Emitted after a page load completes. @p ok is false on failure.
    void onPageLoaded(bool ok);

private:
    QWebEngineView *m_view = nullptr;
};

} // namespace KatHub
