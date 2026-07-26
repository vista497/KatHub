#pragma once

#include <QWidget>
#include <QUrl>
#include <QString>

namespace KatHub {

/// Stub over QWidget for future QWebEngineView integration.
///
/// Currently provides the same API surface as the full WebEngineStub,
/// but without the QWebEngineView dependency — just a plain QWidget.
/// In Phase 4 this will be replaced with a real QWebEngineView wrapper.
class WebEngineStub : public QWidget
{
    Q_OBJECT

public:
    explicit WebEngineStub(QWidget *parent = nullptr);
    ~WebEngineStub() override;

    void loadUrl(const QUrl &url);
    void executeJavaScript(const QString &js);

signals:
    void onPageLoaded(bool ok);
};

} // namespace KatHub
