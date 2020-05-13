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

// Indica quali writers devono essere visualizzati
//void log_set_writers_mask(log_writer_e mask);

// Printa su schermo
void _log_print(log_writer_e writer, log_level_e level, int guid, char* text);

#define LOG_MUTE 1
#if LOG_MUTE
    #define log_info(writer, ...) { char tmp[128]; sprintf(tmp, __VA_ARGS__); _log_print(writer, LOG_LEVEL_INFO,  getpid(), tmp); }
    #define log_warn(writer, ...) { char tmp[128]; sprintf(tmp, __VA_ARGS__); _log_print(writer, LOG_LEVEL_WARN,  getpid(), tmp); }
    #define log_erro(writer, ...) { char tmp[128]; sprintf(tmp, __VA_ARGS__); _log_print(writer, LOG_LEVEL_ERROR, getpid(), tmp); }
#else
    #define log_info(writer, ...)
    #define log_warn(writer, ...)
    #define log_erro(writer, ...)
#endif
