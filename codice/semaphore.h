/// @file semaphore.h
/// @brief Contiene la definizioni di variabili e funzioni
///         specifiche per la gestione dei SEMAFORI.

#pragma once

#include <sys/sem.h>
#include <sys/stat.h>

union semun 
{
	int val;
	struct semid_ds* buf;
	unsigned short* array;
};

int semaphore_create(int count, unsigned short* values);
int mutex_create();

/**
 *  Rimuove sia semafori interi che mutex
 */
void semaphore_remove(int semaphore);

/**
 * Esegue un'operazione sul semaforo intero o sul mutex
 */
void sem_op(int sem, int num, int op);

#define sem_acquire(sem, num) sem_op(sem, num, -1)
#define sem_release(sem, num) sem_op(sem, num,  1)

#define mutex_lock(mutex)   sem_op(mutex, 0, -1)
#define mutex_unlock(mutex) sem_op(mutex, 0,  1)

void semaphore_print(int sem, int count);