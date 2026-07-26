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
#include "HermesSessionsHandler.h"
#include "ModelsHandler.h"
#include "SystemHandler.h"
#include "AgentsHandler.h"
#include "CronHandler.h"
#include "SkillsHandler.h"

// Forward declare dummy functions to force-link non-QObject handlers.
namespace {
    void force_link_vaultgraph() {
        VaultGraphHandler h;
        (void)h.route();
    }
    void force_link_chat() {
        ChatHandler h;
        (void)h.route();
    }
    void force_link_hermes_sessions() {
        HermesSessionsHandler h;
        (void)h.route();
    }
    void force_link_models() {
        ModelsHandler h;
        (void)h.route();
    }
    void force_link_system() {
        SystemHandler h;
        (void)h.route();
    }
    void force_link_agents() {
        AgentsHandler h;
        (void)h.route();
    }
    void force_link_cron() {
        CronHandler h;
        (void)h.route();
    }
    void force_link_skills() {
        SkillsHandler h;
        (void)h.route();
    }
}

void force_handlers_link()
{
    // QObject-derived handlers — reference staticMetaObject.
    volatile auto p1 = &StatusHandler::staticMetaObject;
    volatile auto p3 = &WsStatusHandler::staticMetaObject;
    volatile auto p4 = &PluginListHandler::staticMetaObject;
    (void)p1; (void)p3; (void)p4;

    // Non-QObject handlers — call dummy force-link functions.
    force_link_vaultgraph();
    force_link_chat();
    force_link_hermes_sessions();
    force_link_models();
    force_link_system();
    force_link_agents();
    force_link_cron();
    force_link_skills();
}
