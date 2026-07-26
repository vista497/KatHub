#include "WebEngineStub.h"

namespace KatHub {

WebEngineStub::WebEngineStub(QWidget *parent)
    : QWidget(parent)
{
}

WebEngineStub::~WebEngineStub() = default;

void WebEngineStub::loadUrl(const QUrl & /*url*/)
{
    // Stub — real implementation in Phase 4 with QWebEngineView.
}

void WebEngineStub::executeJavaScript(const QString & /*js*/)
{
    // Stub — real implementation in Phase 4 with QWebEngineView.
}

} // namespace KatHub
