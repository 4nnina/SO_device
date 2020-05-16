/// @file shared_memory.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche per la gestione della MEMORIA CONDIVISA.

#include "err_exit.h"
#include "shared_memory.h"

int shared_memory_create(size_t bytes)
{
    int flags = IPC_CREAT | S_IRUSR | S_IWUSR;
    int result = shmget(IPC_PRIVATE, bytes, flags);
    if (result == -1)
        panic("Errore creazione memoria condivisa");

    return result;
}

void* _shared_memory_attach(int shmem, int flags) 
{
    void* result = shmat(shmem, NULL, flags);
    if (result == (void*)(-1))
        panic("Errore collegamento memoria condivisa");

    return result;
}

void shared_memory_remove(int shmid) 
{
    if (shmctl(shmid, IPC_RMID, NULL) == -1)
        panic("Errore eliminazione memoria condivisa");
}