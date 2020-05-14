#include "device.h"
#include "defines.h"
#include "log.h"
#include "err_exit.h"
#include "semaphore.h"
#include "fifo.h"
#include "shared_memory.h"

#include <unistd.h>
#include <sys/signal.h>
#include <errno.h>

// Dati globali dei devices
static char device_filename[128];
static int  device_fifo_read_fd;

/**
 *  Rimuove risorse aperte dal device
 */
void device_callback_sigterm(int sigterm) {

	log_info("Chiusura fifo in lettura");
	if (close(device_fifo_read_fd) == -1)
		panic("Errore chiusura fifo device lettura");

	log_info("Eliminazione fifo");
	unlink(device_filename);
	
	log_warn("Terminazione device");
	exit(0);
}

// Ottiene prossima coordinata
void get_next_position(int position_file_fd, int* x, int* y) 
{
	char token[3]; // x,y
	
	int read_bytes = read(position_file_fd, token, sizeof(char) * 3);
	if (read_bytes == 0)
	{
		// Resetta posizione nel file
		log_warn("Ricomincio il file posizione");
		lseek(position_file_fd, 0, SEEK_SET);
	
		read_bytes = read(position_file_fd, token, sizeof(char) * 3);
	}
	
	if (read_bytes == -1)
		panic("Errore lettura file posizione")
	else if (read_bytes == 0)
		panic("Errore file posizione vuoto")
	else 
	{
		lseek(position_file_fd, 1, SEEK_CUR);
		sscanf(token, "%d,%d", x, y);
	}
}

/**
 * Prende possesso della scacchiera per settare una cella 
 */
//void checkboard_set(pid_t* checkboard, int mutex, pid_t pid, int x, int y)
//{
//	mutex_lock(mutex);
//	checkboard[x + y * CHECKBOARD_SIDE] = pid;
//	mutex_unlock(mutex);
//}

// ENTRY POINT
int device(int number, device_data_t data)
{
	log_set_levels_mask(LOG_LEVEL_INFO_BIT | LOG_LEVEL_WARN_BIT | LOG_LEVEL_ERROR_BIT);
	log_set_proc_writer(LOG_WRITER_DEVICE);

	log_info("Figlio n. %d", number);
	
	if (signal(SIGTERM, device_callback_sigterm) == SIG_ERR)
		panic("Errore creazione signal handler");

	sprintf(device_filename, "/tmp/dev_fifo.%d", getpid());
	log_info("Creazione fifo %s", device_filename);
	create_fifo(device_filename);
	
	log_info("Apertura fifo %s in lettura", device_filename);
	device_fifo_read_fd = open(device_filename, O_RDONLY);
	if (device_fifo_read_fd == -1)
		panic("Errore apertura fifo '%s' in lettura", device_filename);
	
	// Rendi non bloccante
	int flags = fcntl(device_fifo_read_fd, F_GETFL, 0);
	fcntl(device_fifo_read_fd, F_SETFL, flags | O_NONBLOCK);

	// Attach checkboard e ack list
	log_info("Collegamento memoria condivisa del server");
	pid_t* checkboard = shared_memory_attach(data.checkboard_shme_id, 0, pid_t);
	//ack_t* ack_list = (ack_t*)shmat(data.ack_list_shmem_id, NULL, 0);

	// Messaggi ancora da inviare
	message_t msg_to_send[DEV_MSG_COUNT];
	int msg_count = 0;

	int cur_x = -1, cur_y = -1;
	while(1) 
	{
		// Nota: Questo pezzo di codice funge sia da inizializzazione che da movimento nei prossimi
		// passi del ciclo

		sem_acquire(data.move_sem, number);

		// Avanza la posizione
		int new_x, new_y;
		get_next_position(data.position_file_fd, &new_x, &new_y);

		mutex_lock(data.checkboard_sem);
		{
			static int initialized = 0;
			if (!initialized)
			{
				checkboard[new_x + CHECKBOARD_SIDE * new_y] = getpid();	
				initialized = 1;

				log_info("Device n. %d è in posizione (%d, %d) all'inizio",
					number, new_x, new_y);
			}
			else
			{
				checkboard[cur_x + CHECKBOARD_SIDE * cur_y] = 0;
				checkboard[new_x + CHECKBOARD_SIDE * new_y] = getpid();

				log_info("Device n. %d si è spostato da (%d, %d) a (%d, %d)", 
					number, cur_x, cur_y, new_x, new_y);	
			}
		}
		mutex_unlock(data.checkboard_sem);

		cur_x = new_x;
		cur_y = new_y;

		if (number != DEV_COUNT - 1)
			sem_release(data.move_sem, number + 1);


		// INIZIO CICLO DEVICE

		// sem_wait(data.checkboard_sem, 0);
		// INVIO MESSAGGI
		// sem_signal(data.checkboard_sem, 0);

		msg_count = 0;

		int valid = 1;
		message_t message;
		while (valid && msg_count != DEV_MSG_COUNT)
		{
			size_t bytes_read = read(device_fifo_read_fd, &message, sizeof(message));
			switch (bytes_read)
			{
				// Messaggio valido
				case sizeof(message): {
					
					log_info("Ricevuto un messaggio: %d", message.pid_sender);
					msg_to_send[msg_count] = message;
					msg_count += 1;

				} break;

				// Non ci sono più messaggi
				case 0:
					log_info("Non ci sono più messaggi");
					valid = 0;
					break;

				case -1: {
					if (errno == EAGAIN)
					   valid = 0;
					else
						panic("Errore non definito");	
				} break;

				default:
					panic("Il messaggio è corrotto");
					break;
			} 
		}
	}

	return 0;
}