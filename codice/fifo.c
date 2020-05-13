/// @file fifo.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche per la gestione delle FIFO.

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "err_exit.h"
#include "fifo.h"

void create_fifo(char* filename)
{
	mode_t flags = S_IRUSR | S_IWUSR;
	int fifo = mkfifo(filename, flags);
	if (fifo == -1) {
		ErrExit("Errore creazione fifo");
	}
}
