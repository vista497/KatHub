#include "WebEngineStub.h"

#include <QVBoxLayout>
#include <QWebEngineView>

namespace KatHub {

WebEngineStub::WebEngineStub(QWidget *parent)
    : QWidget(parent)
    , m_view(new QWebEngineView(this))
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_view);

    // Forward QWebEngineView::loadFinished to our onPageLoaded signal.
    connect(m_view, &QWebEngineView::loadFinished,
            this,  &WebEngineStub::onPageLoaded);
}

WebEngineStub::~WebEngineStub() = default;

void WebEngineStub::loadUrl(const QUrl &url)
{
    m_view->load(url);
}

void WebEngineStub::executeJavaScript(const QString &js)
{
    m_view->page()->runJavaScript(js);
}

} // namespace KatHub
