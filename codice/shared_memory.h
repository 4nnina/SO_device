/// @file shared_memory.h
/// @brief Contiene la definizioni di variabili e funzioni
///         specifiche per la gestione della MEMORIA CONDIVISA.

#pragma once

#include <unistd.h>
#include <sys/stat.h>
#include <sys/shm.h>

int  shared_memory_create(size_t bytes);
void shared_memory_remove(int shmid);

void* _shared_memory_attach(int shmid, int flags);
#define shared_memory_attach(shmid, flags, type) \
    (type*)_shared_memory_attach(shmid, flags)

