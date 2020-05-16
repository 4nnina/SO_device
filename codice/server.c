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
#include <time.h>

#include "err_exit.h"
#include "defines.h"
#include "shared_memory.h"
#include "semaphore.h"
#include "log.h"
#include "fifo.h"
#include "device.h"
#include "ack_manager.h"

extern int errno;

// ======================================================================
// Dati globali del server

static pid_t devices_pid[DEV_COUNT];
static int   devices_fifo_fd[DEV_COUNT];

static int ack_manager_pid;

static int position_file_fd;

static int move_sem;
static int checkboard_sem;
static int ack_list_sem;

static int checkboard_shmem_id;
static pid_t* checkboard_shmem;

static int ack_list_shmem_id;
static ack_t*  ack_list_shmen;

// Uccide tutto
void server_callback_sigterm(int sigterm) {

	log_info("Invio sengnale di terminazione ai devices");
	for (int child = 0; child < DEV_COUNT; ++child) {

		// Chiude fifo in lettura del device
		if (devices_fifo_fd[child] != 0 && close(devices_fifo_fd[child]) == -1) {
			panic("Errore chiusura fifo device extra scrittura");
		}
		
		kill(devices_pid[child], SIGTERM);
	}

	log_info("Invio segnale di terminazione all'ack manager");
	kill(ack_manager_pid, SIGTERM);

	int child_exit_status;
	while(waitpid(-1, &child_exit_status, WUNTRACED) != -1) { }
	if (errno != ECHILD) {
		panic("Errore non definito");
	}

	log_info("Chiusura semaforo movimento");
	semaphore_remove(move_sem);

	log_info("Chiusura mutex scacchiera");
	semaphore_remove(checkboard_sem);

	log_info("Chiusura mutex ack list");
	semaphore_remove(ack_list_sem);

	log_info("Eliminazione memoria condivisa scacchiera");
	shared_memory_remove(checkboard_shmem_id);

	log_info("Eliminazione memoria condivisa ack list");
	shared_memory_remove(ack_list_shmem_id);

	log_info("Chiusura file posizioni");
	if (close(position_file_fd) == -1)
		panic("Errore chiusura file posizioni");

	log_warn("Terminazione server");
	exit(0);
}

// Avvia il movimento dei device (2s) e printa
void server_callback_move(int sigalrm) 
{
	time_t timestamp = time(NULL);
	struct tm* print_time = gmtime(&timestamp);

	// Printa su schermo
	printf("\n ----- DEVICES ----------------------- %02d:%02d:%02d  \n",
		print_time->tm_hour, print_time->tm_min, print_time->tm_sec);

	mutex_lock(ack_list_sem);
	for(int i = 0; i < ACK_LIST_MAX_COUNT; ++i)
	{
		ack_t* ack = ack_list_shmen + i;
		if (ack->message_id != 0) 
		{
			struct tm* time = gmtime(&ack->timestamp);
			printf("\tACK: id: %d, sender: %d, receiver: %d, time: %02d:%02d:%02d\n", 
				ack->pid_sender, ack->pid_receiver, ack->message_id, time->tm_hour, time->tm_min, time->tm_sec);
		}
	}
	mutex_unlock(ack_list_sem);

	printf("\n\n");
	sem_release(move_sem, 0);
	alarm(2);
}

int main(int argc, char* argv[]) 
{
	if (argc < 2)
		panic("Usage: %s msg_queue_key file_posizioni [-iwe]", argv[0])
	
	//if (argc >= 3 && strlen(argv[3]))

	// Setta impostazioni del logger se presenti
	int log_level_bits = 0x0;
	if (argc >= 3)
			log_level_bits = log_derive_flags(argv[2]);

	log_set_levels_mask(log_level_bits);
	log_set_proc_writer(LOG_WRITER_SERVER);

	// Blocca tutti i segnali tranne SIGTERM
	sigset_t signals;
	sigfillset(&signals);
	sigdelset(&signals, SIGTERM);
	sigdelset(&signals, SIGALRM);	
	sigprocmask(SIG_SETMASK, &signals, NULL);

	if (signal(SIGTERM, server_callback_sigterm) == SIG_ERR || 
		signal(SIGALRM, server_callback_move) == SIG_ERR)
		panic("Errore creazione signal handlers");

	// Aprire file posizioni
	log_info("Apretura file posizioni");
	position_file_fd = open("input/file_posizioni2.txt", O_RDONLY, S_IRUSR);
	if (position_file_fd == -1)
		panic("Errore apertura file posizioni");

	// Crea memorie condivise

	log_info("Creazione e attach della memoria condivisa per la scacchiera");
	const size_t checkboard_size = sizeof(pid_t) * CHECKBOARD_SIDE * CHECKBOARD_SIDE;
	checkboard_shmem_id = shared_memory_create(checkboard_size);
	checkboard_shmem = shared_memory_attach(checkboard_shmem_id, 0, pid_t);
	memset(checkboard_shmem, 0, checkboard_size);

	log_info("Creazione e attach della memoria condivisa per la lista degli ack");
	const size_t ack_list_size = sizeof(ack_t) * ACK_LIST_MAX_COUNT;
	ack_list_shmem_id = shared_memory_create(ack_list_size);
	ack_list_shmen = shared_memory_attach(ack_list_shmem_id, 0, ack_t);
	memset(ack_list_shmen, 0, ack_list_size);

	// Crea semaforo per scacchiera
	log_info("Creazione mutex scacchiera");
	checkboard_sem = mutex_create();

	// Crea semaforo ack list
	log_info("Creazione mutex ack list");
	ack_list_sem = mutex_create();

	// Crea semaforo per movimento devices
	// NOTA: Il primo è a uno per permettere la prima inizializzazione dei device sulla scacchiera
	log_info("Creazione semaforo movimento dei device");
	unsigned short move_sem_values[DEV_COUNT] = { 1, 0, 0, 0, 0 };
	move_sem = semaphore_create(DEV_COUNT, move_sem_values);

	// Handles dati all'ack manager
	ack_manager_data_t ack_data = { 0 };
	ack_data.log_level_bits = log_level_bits;
	ack_data.ack_list_shmem_id = ack_list_shmem_id;
	ack_data.ack_list_sem = ack_list_sem;

	// TODO rendi robusto
	key_t msg_queue_key = atoi(argv[1]);

	log_info("Creazione processo Ack Manager");
	ack_manager_pid = fork();
	switch(ack_manager_pid)
	{
		// ERRORE
		case -1: {
			panic("Errore creazione ACK Manager");
		} break;

		// Ack manager
		case 0: {
			ack_manager(msg_queue_key, ack_data);
		} break; 
	}
	
	// Handles dati al device
	device_data_t dev_data = {0};
	dev_data.log_level_bits = log_level_bits;
	dev_data.move_sem = move_sem;
	dev_data.position_file_fd = position_file_fd;
	dev_data.checkboard_shme_id = checkboard_shmem_id;
	dev_data.ack_list_shmem_id = ack_list_shmem_id;
	dev_data.checkboard_sem = checkboard_sem;
	dev_data.ack_list_sem = ack_list_sem;

	// Apri ogni fifo dei devices per tenerli in vita
	log_info("Creazioni processi devices");
	for (int child = 0; child < DEV_COUNT; ++child)
	{
		pid_t child_pid = fork();
		switch (child_pid)
		{
			// ERRORE
			case -1: {
				panic("Errore creazione figlio n. %d", child);
			} break;

			// Device
			case 0: {
				return device(child, dev_data);
			} break;
		}

		char child_fifo_filename[128];
		sprintf(child_fifo_filename, "/tmp/dev_fifo.%d", child_pid);

		// TODO: QUESTO FA SCHIFO
		// Potremmo far inviare un messaggio al padre dal figlio appena sta per creare
		// la fifo...
		log_info("Attesa apertura fifo '%s' in scrittura", child_fifo_filename);
		while((devices_fifo_fd[child] = open(child_fifo_filename, O_WRONLY)) == -1);
	}

	// Avvia movimento figli
	alarm(2);

	int child_exit_status;
	while(waitpid(-1, &child_exit_status, WUNTRACED) != -1) { }
	if (errno != ECHILD) {
		panic("Errore non definito");
	}

	return 0; 
}
