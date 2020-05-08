/// @file semaphore.h
/// @brief Contiene la definizioni di variabili e funzioni
///         specifiche per la gestione dei SEMAFORI.

#pragma once

#include <sys/sem.h>
#include <sys/stat.h>

union semun {
	int val;
	struct semid_ds* buf;
	unsigned short* array;
};

int create_semaphore(int num);
void sem_op(int sem, int num, int op);

#define sem_wait(sem, num) \
	sem_op(sem, num, -1)

#define sem_signal(sem, num) \
	sem_op(sem, num, 1)

