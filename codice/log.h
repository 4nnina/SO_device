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

// Indica quali messaggi devono essere visualizzati
void log_set_levels_mask(log_level_e mask);

// Setta il writer di questo processo
void log_set_proc_writer(log_writer_e writer);

// Printa su schermo
void _log_print(log_level_e level, int guid, char* text);

#define LOG_TMP_BUFFER_SIZE 516

#if LOG_MUTE
    #define log_info(...)
    #define log_warn(...)
    #define log_erro(...)
#else
    #define log_info(...) { char tmp[LOG_TMP_BUFFER_SIZE]; sprintf(tmp, __VA_ARGS__); _log_print(LOG_LEVEL_INFO,  getpid(), tmp); }
    #define log_warn(...) { char tmp[LOG_TMP_BUFFER_SIZE]; sprintf(tmp, __VA_ARGS__); _log_print(LOG_LEVEL_WARN,  getpid(), tmp); }
    #define log_erro(...) { char tmp[LOG_TMP_BUFFER_SIZE]; sprintf(tmp, __VA_ARGS__); _log_print(LOG_LEVEL_ERROR, getpid(), tmp); }    
#endif
