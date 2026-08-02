#pragma once

#include "IHttpHandler.h"

#include <string>

class SpeechManager;

// Built-in handler: POST /api/speech
// Команды управления речевым слоем (STT/TTS):
//   {"command":"capture","seconds":30}        → распознать последние N сек из буфера → {"text":"...","seconds":30}
//   {"command":"ptt","pressed":true}          → PTT: включить/выключить стриминг в STT → {"ok":true}
//   {"command":"speak","text":"Привет"}       → озвучить текст через TTS → {"ok":true}
//   {"command":"status"}                      → состояние → {"streaming":false,"enabled":true}
class SpeechHandler : public IHttpHandler
{
public:
    SpeechHandler();

    const char* route() override;
    HttpMethod  method() override;
    void        handle(const char* request, void* response) override;
    void        handleWithContext(const char* body, const char* path,
                                  const char* query, void* response,
                                  const char* method = nullptr) override;

    void setSpeechManager(SpeechManager *mgr) { speechManager_ = mgr; }

private:
    SpeechManager *speechManager_ = nullptr;
};
