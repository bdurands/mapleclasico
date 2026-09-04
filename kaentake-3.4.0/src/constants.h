#pragma once

#define CONSTANTS_WINDOW_NAME "EllinMS"
#define CONSTANTS_DLL_NAME    "ellinms.dll"
#define CONSTANTS_CONFIG_NAME "config.ini"

#define CONSTANTS_CENTER_STATUSBAR FALSE

#define CONSTANTS_DEFAULT_HOST     "127.0.0.1"
#define CONSTANTS_USE_COMMAND_LINE TRUE
#define CONSTANTS_USE_CONFIG_FILE  TRUE

extern char* g_sServerHost;
extern long g_nServerPort;

#define CONSTANTS_DAMAGE_CAP 2147483647.0