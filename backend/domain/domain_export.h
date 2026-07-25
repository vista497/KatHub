#pragma once

// DLL export/import macros for KatHub domain interfaces.
// Plugins (DLLs) use these to properly export/import symbols.

#ifdef _WIN32
    #ifdef KATHUB_DOMAIN_BUILD_DLL
        #define KATHUB_DOMAIN_EXPORT __declspec(dllexport)
    #else
        #define KATHUB_DOMAIN_EXPORT __declspec(dllimport)
    #endif
#else
    #define KATHUB_DOMAIN_EXPORT
#endif
