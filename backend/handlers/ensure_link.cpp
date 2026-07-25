// Force linker to include all object files from kathub-handlers.
// MSVC discards static lib objects if no symbol is referenced.
// This ensures REGISTER_HANDLER globals in handler .cpp files execute.
//
// Pitfall: handlers that don't inherit QObject have no staticMetaObject.
// For those, we forward-declare and take address of any method.

#include "StatusHandler.h"
#include "VaultGraphHandler.h"
#include "WsStatusHandler.h"
#include "PluginListHandler.h"
#include "ChatHandler.h"

// Forward declare dummy functions to force-link non-QObject handlers.
namespace {
    void force_link_vaultgraph() {
        // Force linker to pull in the VaultGraphHandler translation unit.
        // REGISTER_HANDLER creates a static init object; referencing ANY
        // symbol from the TU (e.g. the class itself) forces the linker
        // to include it.
        VaultGraphHandler h;
        (void)h.route();
    }
}

void force_handlers_link()
{
    // QObject-derived handlers — reference staticMetaObject.
    volatile auto p1 = &StatusHandler::staticMetaObject;
    volatile auto p3 = &WsStatusHandler::staticMetaObject;
    volatile auto p4 = &PluginListHandler::staticMetaObject;
    volatile auto p5 = &ChatHandler::staticMetaObject;
    (void)p1; (void)p3; (void)p4; (void)p5;

    // Non-QObject handlers — call dummy force-link function.
    force_link_vaultgraph();
}
