#include "WebViewWidget.h"

#include <QVBoxLayout>

namespace kathub {

WebViewWidget::WebViewWidget(QWidget* parent)
    : QWidget(parent)
    , m_webView(new QWebEngineView(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_webView);
}

void WebViewWidget::loadUrl(const QString& url)
{
    m_webView->load(QUrl(url));
}

} // namespace kathub
