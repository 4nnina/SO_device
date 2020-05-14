/// @file semaphore.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche per la gestione dei SEMAFORI.

#include "err_exit.h"
#include "semaphore.h"

#include <unistd.h>

int semaphore_create(int count, unsigned short* values)
{
	int flags = S_IRUSR | S_IWUSR | S_IWGRP | S_IRGRP;
	int semaphore = semget(IPC_PRIVATE, count, flags);
	if (semaphore == -1)
		panic("Errore creazione semaforo");

	union semun arg;
    arg.array = values;
    if (semctl(semaphore, 0, SETALL, arg) == -1)
        panic("Errore creazione semaforo");

	return semaphore;
}

int mutex_create()
{
	unsigned short values[1] = { 1 };
	int semaphore = semaphore_create(1, values); 
	return semaphore;
}

void semaphore_remove(int semaphore)
{
	if (semctl(semaphore, 0, IPC_RMID, NULL) == -1)
		panic("Errore chiusura semaforo");
}

void sem_op(int sem, int num, int op) 
{
	struct sembuf arg = { num, op, 0 };
	if (semop(sem, &arg, 1) == -1)
		panic("Errore operazione semaforo");
}

void semaphore_print(int sem, int count) 
{
	unsigned short values[32];
	union semun arg;
	arg.array = values;

	if (semctl(sem, 0, GETALL, arg) == -1)
		panic("Errore ottenimento valore semaforo");

	log_info("Valore semaforo:");
	for (int i = 0; i < count; ++i)
		printf(" %d ", values[i]);
	putchar('\n');
}
