/// @file semaphore.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche per la gestione dei SEMAFORI.

#include "err_exit.h"
#include "semaphore.h"

#include <unistd.h>

int create_semaphore(int num) {
	int sem = semget(IPC_PRIVATE, num, S_IRUSR | S_IWUSR);
	if (sem == -1)
		ErrExit("Errore creazione semaforo");

	return sem;	
}

void sem_op(int sem, int num, int op) {
	struct sembuf arg = { num, op, 0 };
	if (semop(sem, &arg, 1) == -1)
		ErrExit("Errore operazione semaforo");
}
