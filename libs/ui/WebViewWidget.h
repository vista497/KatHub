#pragma once

#include <QWebEngineView>
#include <QWidget>

namespace kathub {

class WebViewWidget : public QWidget {
    Q_OBJECT
public:
    explicit WebViewWidget(QWidget* parent = nullptr);

    void loadUrl(const QString& url);

private:
    QWebEngineView* m_webView = nullptr;
};

} // namespace kathub
