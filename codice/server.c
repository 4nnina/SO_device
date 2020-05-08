/// @file server.c
/// @brief Contiene l'implementazione del SERVER.

#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <fcntl.h>

#include "err_exit.h"
#include "defines.h"
#include "shared_memory.h"
#include "semaphore.h"
#include "fifo.h"

#define DEV_COUNT 5
#define DEV_MSG_COUNT 32

extern int errno;

// ENTRY POINT
int device(int number)
{
	pid_t pid = getpid();
	printf("Child n. %d => - %d -\n", number, pid);
	
	char filename[128];
	sprintf(filename, "/tmp/dev_fifo.%d", pid);
	
	create_fifo(filename);
	
	int fifo_read_fd = open(filename, O_RDONLY);
	if (fifo_read_fd == -1)
		panic("%d | Errore apertura fifo '%s' in lettura", pid, filename);

	// Mantiene viva la fifo anche se non ci sono 
	// client o messaggi
	int fifo_write_fd = open(filename, O_WRONLY);
	if (fifo_write_fd == -1)
		panic("%d | Errore apertura fifo '%s' in scrittura", pid, filename);

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
			int bytes_read = read(fifo_read_fd, &message, sizeof(message));
			switch (bytes_read)
			{
				// Nuovo messaggio valido
				case sizeof(message): {
					
					printf("%d ha ricevuto un messaggio (%d)\n", 
							pid, message.pid_sender);

					msg_to_send[msg_count] = message;
					msg_count += 1;

				} break;

				// Non ci sono più messaggi
				case 0:
					valid = 0;
					break;

				default:
					panic("%d | Il messaggio è corrotto", pid);
					break;
			} 
		}

		// PRENDI SEMAFORO SCACCHIERA
		// PRENDI SEMAFORO TURNO TUO
		// MOVIMENTO
		// LIBERA SEMAFORO TURNO ALTRO
		// LASCIA SEMAFORO SCACCHIERA

	}

	return 0;
}

// ENTRY POINT
int ack_manager()
{
	return 0;
}

int main(int argc, char * argv[]) {
    
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

	pid_t devices_pid[DEV_COUNT];
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
	}

	pid_t child;
	while((child = wait(NULL)) != -1) { }
	
	if (errno != ECHILD) {
		panic("%d | Errore non definito", getpid());
	}
	
	return 0;
}
