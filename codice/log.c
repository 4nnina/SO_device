#include "log.h"

#include <stdio.h>

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