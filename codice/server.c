/// @file server.c
/// @brief Contiene l'implementazione del SERVER.

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <memory.h>

#include "err_exit.h"
#include "defines.h"
#include "shared_memory.h"
#include "semaphore.h"
#include "log.h"
#include "fifo.h"
#include "device.h"

extern int errno;


// ENTRY POINT
int ack_manager()
{
	return 0;
}

// ======================================================================

// Dati globali del server
pid_t devices_pid[DEV_COUNT];
int devices_fifo_fd[DEV_COUNT];
int position_file_fd;

int devices_move_sem;
int checkboard_sem;

pid_t* checkboard_shmem;

// Uccide tutto
void server_callback_sigterm(int sigterm) {

	log_info(LOG_WRITER_SERVER, "Invio sengnale di terminazione ai devices");
	for (int child = 0; child < DEV_COUNT; ++child) {

		// Chiude fifo in lettura del device
		if (devices_fifo_fd[child] != 0 && close(devices_fifo_fd[child]) == -1) {
			panic(LOG_WRITER_SERVER, "Errore chiusura fifo device extra scrittura");
		}
		
		kill(devices_pid[child], SIGTERM);
	}

	// TODO ack manager

	int child_exit_status;
	while(waitpid(-1, &child_exit_status, WUNTRACED) != -1) { }
	if (errno != ECHILD) {
		panic(LOG_WRITER_SERVER, "Errore non definito");
	}

	log_info(LOG_WRITER_SERVER, "Chiusura semaforo movimento");
	close_semaphore(devices_move_sem);

	log_info(LOG_WRITER_SERVER, "Chiusura file posizioni");
	if (close(position_file_fd) == -1)
		panic(LOG_WRITER_SERVER, "Errore chiusura file posizioni");

	log_warn(LOG_WRITER_SERVER, "Terminazione server");
	exit(0);
}

// Avvia il movimento dei device (2s) e printa
void server_callback_move(int sigalrm) {

	// Printa su schermo
	int x,y;
	for(int device = 0; device < DEV_COUNT; ++device ) {
		printf("%d\n", devices_pid[device]);
	}

	sem_signal(devices_move_sem, 0);	
	alarm(2);
}

int main(int argc, char * argv[]) {

	log_set_levels_mask(LOG_LEVEL_INFO_BIT | LOG_LEVEL_WARN_BIT | LOG_LEVEL_ERROR_BIT);

	// Blocca tutti i segnali tranne SIGTERM
	sigset_t signals;
	sigfillset(&signals);
	sigdelset(&signals, SIGTERM);
	sigdelset(&signals, SIGALRM);	
	sigprocmask(SIG_SETMASK, &signals, NULL);

	if (signal(SIGTERM, server_callback_sigterm) == SIG_ERR || 
		signal(SIGALRM, server_callback_move) == SIG_ERR)
		panic(LOG_WRITER_SERVER, "Errore creazione signal handlers");

	// Aprire file posizioni
	log_info(LOG_WRITER_SERVER, "Apretura file posizioni");
	position_file_fd = open("input/file_posizioni.txt", O_RDONLY, S_IRUSR);
	if (position_file_fd == -1)
		panic(LOG_WRITER_SERVER, "Errore apertura file posizioni");

	// Crea memorie condivise

	log_info(LOG_WRITER_SERVER, "Creazione e attach della memoria condivisa per la scacchiera");
	const size_t checkboard_size = sizeof(pid_t) * CHECKBOARD_SIDE * CHECKBOARD_SIDE;
	int checkboard_shmem_id = create_shared_memory(checkboard_size);
	checkboard_shmem = (pid_t*)shmat(checkboard_shmem_id, NULL, 0);
	memset(checkboard_shmem, 0, checkboard_size);

	log_info(LOG_WRITER_SERVER, "Creazione e attach della memoria condivisa per la lista degli ack");
	const size_t ack_list_size = sizeof(ack_t) * ACK_LIST_MAX_COUNT;
	int ack_list_shmem_id = create_shared_memory(ack_list_size);
	ack_t*  ack_list_shmen = (ack_t*)shmat(ack_list_shmem_id, NULL, 0);
	memset(ack_list_shmen, 0, ack_list_size);

	// Crea semaforo per movimento devices

	log_info(LOG_WRITER_SERVER, "Creazione semaforo movimento dei device");
	devices_move_sem = create_semaphore(DEV_COUNT);
	// NOTA: Il primo è a uno per permettere la prima inizializzazione dei device sulla scacchiera
    unsigned short sem_init_val[DEV_COUNT] = { 1, 0, 0, 0, 0 };
	memset(sem_init_val, 0, DEV_COUNT * sizeof(*sem_init_val));

    union semun arg;
    arg.array = sem_init_val;
    if (semctl(devices_move_sem, 0, SETALL, arg) == -1)
        panic(LOG_WRITER_SERVER, "Errore creazione semaforo");

	// Crea semaforo per scacchiera

	log_info(LOG_WRITER_SERVER, "Creazione mutex scacchiera");
	checkboard_sem = create_semaphore(1);
	sem_init_val[0] = 1;
	
    arg.array = sem_init_val;
    if (semctl(checkboard_sem, 0, SETALL, arg) == -1)
        panic(LOG_WRITER_SERVER, "Errore creazione mutex scacchiera");

	// Crea semaforo ack list
	log_info(LOG_WRITER_SERVER, "Creazione mutex ack list");
	int ack_list_sem = create_semaphore(1);
	sem_init_val[0] = 1;

    arg.array = sem_init_val;
    if (semctl(ack_list_sem, 0, SETALL, arg) == -1)
        panic(LOG_WRITER_SERVER, "Errore creazione mutex ack list");


	// Timer
	alarm(2);

	log_info(LOG_WRITER_SERVER, "Creazione processo Ack Manager");
	pid_t ack_manager_pid = fork();
	switch(ack_manager_pid)
	{
		// ERRORE
		case -1: {
			panic(LOG_WRITER_SERVER, "Errore creazione ACK Manager");
		} break;

		// Ack manager
		case 0: {
			return ack_manager();
		} break; 
	}
	
	// Handles dati al device
	device_data_t dev_data = {0};
	dev_data.move_sem = devices_move_sem;
	dev_data.position_file_fd = position_file_fd;
	dev_data.checkboard_shme_id = checkboard_shmem_id;
	dev_data.ack_list_shmem_id = ack_list_shmem_id;
	dev_data.checkboard_sem = checkboard_sem;
	dev_data.ack_list_sem = ack_list_sem;

	// Apri ogni fifo dei devices per tenerli in vita
	log_info(LOG_WRITER_SERVER, "Creazioni processi devices");
	for (int child = 0; child < DEV_COUNT; ++child)
	{
		pid_t child_pid = fork();
		switch (child_pid)
		{
			// ERRORE
			case -1: {
				panic(LOG_WRITER_SERVER, "Errore creazione figlio n. %d", child);
			} break;

			// Device
			case 0: {
				return device(child, dev_data);
			} break;
		}
	}

	int child_exit_status;
	while(waitpid(-1, &child_exit_status, WUNTRACED) != -1) { }
	if (errno != ECHILD) {
		panic(LOG_WRITER_SERVER, "Errore non definito");
	}

	return 0; 
}
