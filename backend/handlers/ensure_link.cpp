// Force linker to include all object files from kathub-handlers.
// MSVC discards static lib objects if no symbol is referenced.
// This ensures REGISTER_HANDLER globals in handler .cpp files execute.
#include "StatusHandler.h"
void force_handlers_link() { volatile auto p = &StatusHandler::staticMetaObject; (void)p; }
