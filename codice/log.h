#pragma once

#include <unistd.h>

typedef enum log_writer_e {

    LOG_WRITER_ACKMAN,
    LOG_WRITER_SERVER,
    LOG_WRITER_DEVICE,

    LOG_WRITERS_COUNT

} log_writer_e;

typedef enum log_level_e {

    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR,

    LOG_LEVELS_COUNT

} log_level_e;

#define LOG_LEVEL_INFO_BIT  (1 << LOG_LEVEL_INFO)
#define LOG_LEVEL_WARN_BIT  (1 << LOG_LEVEL_WARN)
#define LOG_LEVEL_ERROR_BIT (1 << LOG_LEVEL_ERROR)

// Colore del guid dei messaggi (unix style)
static const char* log_guid_color = "\e[0;32;2m"; // Verde

// Nome dello scrittore
static const char* log_writers_text[LOG_WRITERS_COUNT] = 
{
    "ACKMAN", 
    "SERVER", 
    "DEVICE"
};

// Colore del nome dello scrittore (unix style)
static const char* log_writers_color[LOG_WRITERS_COUNT] = 
{
    "\e[38;5;129m",     // ACKMAN: Viola
    "\e[38;5;118m",     // SERVER: Verde
    "\e[38;5;81m",      // DEVICE: Bluastro
};

// Nome del livello
static const char* log_levels_text[LOG_LEVELS_COUNT] = 
{
    "INFO", 
    "WARN", 
    "ERR"
};

// Colore dei messaggi in base al livello
static const char* log_levels_color[LOG_LEVELS_COUNT] = 
{
    "\e[m",         // INFO: Bianco
    "\e[0;33m",     // WARN: Marrone 
    "\e[0;31m"      // ERROR: Rosso
};

// Indica quali messaggi devono essere visualizzati
void log_set_levels_mask(log_level_e mask);

// Setta il writer di questo processo
void log_set_proc_writer(log_writer_e writer);

// Printa su schermo
void _log_print(log_level_e level, int guid, char* text);

#if LOG_MUTE
    #define log_info(...)
    #define log_warn(...)
    #define log_erro(...)
#else
    #define log_info(...) { char tmp[128]; sprintf(tmp, __VA_ARGS__); _log_print(LOG_LEVEL_INFO,  getpid(), tmp); }
    #define log_warn(...) { char tmp[128]; sprintf(tmp, __VA_ARGS__); _log_print(LOG_LEVEL_WARN,  getpid(), tmp); }
    #define log_erro(...) { char tmp[128]; sprintf(tmp, __VA_ARGS__); _log_print(LOG_LEVEL_ERROR, getpid(), tmp); }    
#endif
