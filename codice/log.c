#include "log.h"

#include <stdio.h>
#include <string.h>

// Colore del guid dei messaggi (unix style)
const char* log_guid_color = "\e[0;32;2m"; // Verde

// Nome dello scrittore
static const char* log_writers_text[LOG_WRITERS_COUNT] = 
{
    "ACKMAN", 
    "SERVER", 
    "DEVICE",
    "CLIENT",
};

// Colore del nome dello scrittore (unix style)
static const char* log_writers_color[LOG_WRITERS_COUNT] = 
{
    "\e[38;5;129m",     // ACKMAN: Viola
    "\e[38;5;118m",     // SERVER: Verde
    "\e[38;5;81m",      // DEVICE: Bluastro
    "\e[38;5;190m",     // DEVICE: Giallo
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

static log_writer_e global_proc_writer = 0;
static unsigned int global_levels_mask = 0;

void log_set_levels_mask(unsigned int mask) 
{
    global_levels_mask = mask;
}

void log_set_proc_writer(log_writer_e writer)
{
    global_proc_writer = writer;
}

void _log_print(log_level_e level, int guid, char* text)
{
    int show_level  = ((1 << level) & global_levels_mask) != 0;
    if (show_level) 
    {
        //                LIVELLO      SCRITTORE   GUID         MESSAGGIO
        fprintf(stdout, "[%s %4s \e[m][%s %6s \e[m][%s %5d \e[m]: %s %s \e[m\n",
            log_levels_color[level], log_levels_text[level],
            log_writers_color[global_proc_writer], log_writers_text[global_proc_writer],
            log_guid_color, guid,
            log_levels_color[level], text);
    }
}

log_level_e log_derive_flags(char* flag)
{
    log_level_e result = 0x0;
	for(int i = 1; i < strlen(flag); ++i)
		switch (flag[i]) {
			case 'i': result |= LOG_LEVEL_INFO_BIT;  break;
			case 'w': result |= LOG_LEVEL_WARN_BIT;  break;
			case 'e': result |= LOG_LEVEL_ERROR_BIT; break;
		}
    return result;
}