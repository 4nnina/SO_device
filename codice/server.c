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
#include "fifo.h"

#define DEV_COUNT 5
#define DEV_MSG_COUNT 32

extern int errno;

#define FIFO_READ  0
#define FIFO_WRITE 1

// Server
pid_t devices_pid[DEV_COUNT];

int  devices_move_sem;
char device_filename[126];
int  device_fifo_read_fd;

void device_callback_sigterm(int sigterm) {

	printf("%d (device) si sta chiudendo\n", getpid());
	if (close(device_fifo_read_fd) == -1) {
		panic("%d | Errore chiusura fifo device lettura: %d", getpid(), errno);	
	}

	

	unlink(device_filename);
	exit(0);
}

// ENTRY POINT
int device(int number)
{
	pid_t pid = getpid();
	printf("Child n. %d => - %d -\n", number, pid);
	
	if (signal(SIGTERM, device_callback_sigterm) == SIG_ERR)
		panic("%d | Errore creazione signal handler", getpid());

	sprintf(device_filename, "/tmp/dev_fifo.%d", pid);	
	printf("%d | Creazione fifo a '%s'\n", pid, device_filename);
	create_fifo(device_filename);
	
	printf ("%d | Apertura fifo in lettura\n", pid);
	device_fifo_read_fd = open(device_filename, O_RDONLY);
	if (device_fifo_read_fd == -1)
		panic("%d | Errore apertura fifo '%s' in lettura", pid, device_filename);
	
	// Rendi non bloccante
	int flags = fcntl(device_fifo_read_fd, F_GETFL, 0);
	fcntl(device_fifo_read_fd, F_SETFL, flags | O_NONBLOCK);

	// Messaggi ancora da inviare
	message_t msg_to_send[DEV_MSG_COUNT];
	int msg_count = 0;	


	// TODO: Semafori
	while(1) {

		// PRENDI SEMAFORO SCACCHIERA
		// INVIO MESSAGGI
		// LASCIA SEMAFORO SCACCHIERA

		msg_count = 0;

		// LEGGI MESSAGGI

		int valid = 1;
		message_t message;
		while (valid && msg_count != DEV_MSG_COUNT)
		{
			int bytes_read = read(device_fifo_read_fd, &message, sizeof(message));
			switch (bytes_read)
			{
				// Messaggio valido
				case sizeof(message): {
					
					printf("%d ha ricevuto un messaggio (%d)\n", 
							pid, message.pid_sender);

					msg_to_send[msg_count] = message;
					msg_count += 1;

				} break;

				// Non ci sono più messaggi
				case 0:
					printf("%d non ci sono più messaggi\n", pid);
					valid = 0;
					break;

				case -1: {
					if (errno == EAGAIN)
					   valid = 0;
					else
						panic("%d | Errore non definito", pid);	
				} break;

				default:
					panic("%d | Il messaggio è corrotto", pid);
					break;
			} 
		}

		// PRENDI SEMAFORO SCACCHIERA
		//printf("%d | Aspetta semaforo\n", pid);

		// PRENDI SEMAFORO TURNO TUO
		sem_wait(devices_move_sem, number);
		

		//printf("Il device n. %d si è mosso\n", number);
		// MOVIMENTO
		
		// LIBERA SEMAFORO TURNO ALTRO
		if (number != DEV_COUNT - 1)
			sem_signal(devices_move_sem, number + 1);

		// LASCIA SEMAFORO SCACCHIERA

	}

	return 0;
}

// ENTRY POINT
int ack_manager()
{
	return 0;
}

// ======================================================================

int devices_fifo_fd[DEV_COUNT];

// Uccide tutto
void server_callback_sigterm(int sigterm) {

	printf("Server si sta chiudendo\n");

	for (int child = 0; child < DEV_COUNT; ++child) {

		// Chiude fifo in lettura del device
		if (devices_fifo_fd[child] != 0 && close(devices_fifo_fd[child]) == -1) {
			panic("%d | Errore chiusura fifo device extra scrittura", getpid());
		}
		
		kill(devices_pid[child], SIGTERM);
	}

	// TODO ack manager



	// ELIMINA SEMAFORO	

	exit(0);
}



// Avvia il movimento dei device (2s)
void server_callback_move(int sigalrm) {

	//printf("Avvio movimento (2s)\n");	
	sem_signal(devices_move_sem, 0);	
	alarm(2);
}

int main(int argc, char * argv[]) {

	// Blocca tutti i segnali tranne SIGTERM
	sigset_t signals;
	sigfillset(&signals);
	sigdelset(&signals, SIGTERM);
	sigdelset(&signals, SIGALRM);	
	sigprocmask(SIG_SETMASK, &signals, NULL);

	if (signal(SIGTERM, server_callback_sigterm) == SIG_ERR || 
		signal(SIGALRM, server_callback_move) == SIG_ERR)
		panic("%d | Errore creazione signal handlers", getpid());

	// Crea semaforo per movimento devices

	devices_move_sem = create_semaphore(DEV_COUNT);
    unsigned short sem_init_val[DEV_COUNT];
	memset(sem_init_val, 0, DEV_COUNT * sizeof(*sem_init_val));

    union semun arg;
    arg.array = sem_init_val;
    if (semctl(devices_move_sem, 0, SETALL, arg) == -1)
        panic("%d | Errore creazione semaforo", getpid());

	// 

	alarm(2);

	pid_t ack_manager_pid = fork();
	switch(ack_manager_pid)
	{
		// ERRORE
		case -1: {
			panic("%d | Errore creazione ACK Manager", getpid());
		} break;

		// Ack manager
		case 0: {
			return ack_manager();
		} break; 
	}
	
	// Apri ogni fifo dei devices per tenerli in vita
	for (int child = 0; child < DEV_COUNT; ++child)
	{
		pid_t child_pid = fork();	
		switch (child_pid)
		{
			// ERRORE
			case -1: {
				panic("%d | Errore creazione figlio n. %d", getpid(), child);
			} break;

			// Device
			case 0: {
				devices_pid[child] = child_pid;
				return device(child);
			} break;
		}
 
		char device_filename[126];
		sprintf(device_filename, "/tmp/dev_fifo.%d", child_pid);

		// TODO: Questo fa schifo
		while ((devices_fifo_fd[child] = open(device_filename, O_WRONLY)) == -1);
		if (devices_fifo_fd[child] == -1)
			panic("%d | Errore apertura fifo '%s' in scrittura", getpid(), device_filename);

	}

	int child_exit_status;
	while(waitpid(-1, &child_exit_status, WUNTRACED) != -1) { }
	if (errno != ECHILD) {
		panic("%d | Errore non definito", getpid());
	}

	return 0;
}
