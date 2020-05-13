/// @file shared_memory.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche per la gestione della MEMORIA CONDIVISA.

#include "err_exit.h"
#include "shared_memory.h"

int create_shared_memory(size_t bytes)
{
    int flags = S_IRUSR | S_IWUSR;
    int mem = shmget(IPC_PRIVATE, bytes, flags);
    if (mem == -1)
        ErrExit("Errore creazione memoria condivisa");

    return mem;
}