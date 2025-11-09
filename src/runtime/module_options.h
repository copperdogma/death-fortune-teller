#ifndef RUNTIME_MODULE_OPTIONS_H
#define RUNTIME_MODULE_OPTIONS_H

// Compile-time toggles for runtime modules. Set these via build flags or
// by defining them before including this header.
// Example: add -DAPP_ENABLE_WIFI=0 to disable Wi-Fi for a given prop.

#ifndef APP_ENABLE_CLI
#define APP_ENABLE_CLI 1
#endif

#ifndef APP_ENABLE_CONTENT_SELECTION
#define APP_ENABLE_CONTENT_SELECTION 1
#endif

#ifndef APP_ENABLE_PRINTER
#define APP_ENABLE_PRINTER 1
#endif

#ifndef APP_ENABLE_CONNECTIVITY
#define APP_ENABLE_CONNECTIVITY 1
#endif

#ifndef APP_ENABLE_WIFI
#define APP_ENABLE_WIFI APP_ENABLE_CONNECTIVITY
#endif

#ifndef APP_ENABLE_OTA
#define APP_ENABLE_OTA APP_ENABLE_CONNECTIVITY
#endif

#ifndef APP_ENABLE_REMOTE_DEBUG
#define APP_ENABLE_REMOTE_DEBUG APP_ENABLE_CONNECTIVITY
#endif

#ifndef APP_ENABLE_BLUETOOTH
#define APP_ENABLE_BLUETOOTH 1
#endif

#endif  // RUNTIME_MODULE_OPTIONS_H

