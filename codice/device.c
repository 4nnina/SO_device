#include "device.h"
#include "defines.h"
#include "log.h"
#include "err_exit.h"
#include "semaphore.h"
#include "fifo.h"

#include <unistd.h>
#include <sys/signal.h>
#include <errno.h>

// Dati globali dei devices
static char device_filename[126];
static int  device_fifo_read_fd;

void device_callback_sigterm(int sigterm) {

	log_info(LOG_WRITER_DEVICE, "Chiusura fifo in lettura");
	if (close(device_fifo_read_fd) == -1) {
		panic(LOG_WRITER_DEVICE, "Errore chiusura fifo device lettura");	
	}

	log_info(LOG_WRITER_DEVICE, "Eliminazione fifo");
	unlink(device_filename);
	
	log_warn(LOG_WRITER_DEVICE, "Terminazione device");
	exit(0);
}

// Ottiene prossima coordinata
void get_next_position(int position_file_fd, int* x, int* y) {
	char token[3]; // 0,0
	
	int read_bytes = read(position_file_fd, token, sizeof(char) * 3);
	if (read_bytes == 0)
	{
		// Resetta posizione nel file
		log_info(LOG_WRITER_DEVICE, "Ricomincio il file posizione");
		lseek(position_file_fd, 0, SEEK_SET);
	
		read_bytes = read(position_file_fd, token, sizeof(char) * 3);
	}
	
	if (read_bytes == -1)
		panic(LOG_WRITER_DEVICE, "Errore lettura file posizione")
	else if (read_bytes == 0)
		panic(LOG_WRITER_DEVICE, "Errore file posizione vuoto")
	else {
		lseek(position_file_fd, 1, SEEK_CUR);
		sscanf(token, "%d,%d", x, y);
	}
}

void set_checkboard_position_mtx(pid_t* checkboard, int mtx, pid_t pid, int cur_x, int cur_y, int new_x, int new_y)
{
	sem_wait(mtx, 0);
	checkboard[cur_x + CHECKBOARD_SIDE * cur_x] = 0;
	checkboard[new_y + CHECKBOARD_SIDE * new_y] = pid;
	sem_signal(mtx, 0);
}

// ENTRY POINT
int device(int number, device_data_t data)
{
	log_info(LOG_WRITER_DEVICE, "Figlio n. %d", number);
	
	if (signal(SIGTERM, device_callback_sigterm) == SIG_ERR)
		panic(LOG_WRITER_DEVICE, "Errore creazione signal handler");

	sprintf(device_filename, "/tmp/dev_fifo.%d", getpid());
	log_info(LOG_WRITER_DEVICE, "Creazione fifo %s", device_filename);
	create_fifo(device_filename);
	
	log_info(LOG_WRITER_DEVICE, "Apertura fifo %s in lettura", device_filename);
	device_fifo_read_fd = open(device_filename, O_RDONLY);
	if (device_fifo_read_fd == -1)
		panic(LOG_WRITER_DEVICE, "Errore apertura fifo '%s' in lettura", device_filename);
	
	// Rendi non bloccante
	int flags = fcntl(device_fifo_read_fd, F_GETFL, 0);
	fcntl(device_fifo_read_fd, F_SETFL, flags | O_NONBLOCK);

	// Attach checkboard e ack list
	log_info(LOG_WRITER_DEVICE, "Collegamento memroia condivisa del server");
	pid_t* checkboard = (pid_t*)shmat(data.checkboard_shme_id, NULL, 0);
	ack_t* ack_list = (ack_t*)shmat(data.ack_list_shmem_id, NULL, 0);

	// Messaggi ancora da inviare
	message_t msg_to_send[DEV_MSG_COUNT];
	int msg_count = 0;
	
	//Inizializzazione scacchiera
	int cur_x = 0, cur_y = 0;

	sem_wait(data.move_sem, number);
		
	// Avanza la posizione
	get_next_position(data.position_file_fd, &cur_x, &cur_y);
	
	//set_checkboard_position_mtx(checkboard, data.checkboard_sem, getpid(), 0, 0, cur_x, cur_y);
	sem_wait(data.checkboard_sem, 0);
	checkboard[cur_x + CHECKBOARD_SIDE * cur_x] = 0;
	sem_signal(data.checkboard_sem, 0);
	//log_info(LOG_WRITER_DEVICE, "Device n. %d è alla posizione (%d, %d)", 
	//	number, cur_x, cur_y);

	// LIBERA SEMAFORO TURNO ALTRO
	if (number != DEV_COUNT - 1)
		sem_signal(data.move_sem, number + 1);

	while(1) 
	{
		sem_wait(data.checkboard_sem, 0);
		// INVIO MESSAGGI
		sem_signal(data.checkboard_sem, 0);

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
					
					log_info(LOG_WRITER_DEVICE, "Ricevuto un messaggio: %d", message.pid_sender);
					msg_to_send[msg_count] = message;
					msg_count += 1;

				} break;

				// Non ci sono più messaggi
				case 0:
					log_info(LOG_WRITER_DEVICE, "Non ci sono più messaggi");
					valid = 0;
					break;

				case -1: {
					if (errno == EAGAIN)
					   valid = 0;
					else
						panic(LOG_WRITER_DEVICE, "Errore non definito");	
				} break;

				default:
					panic(LOG_WRITER_DEVICE, "Il messaggio è corrotto");
					break;
			} 
		}

		sem_wait(data.move_sem, number);
		
		// Avanza la posizione
		int new_x, new_y;
		get_next_position(data.position_file_fd, &new_x, &new_y);

		set_checkboard_position_mtx(checkboard, data.checkboard_sem, getpid(), cur_x, cur_y, new_x, new_y);
		
		sem_wait(data.checkboard_sem, 0);
		checkboard[cur_x + CHECKBOARD_SIDE * cur_x] = 0;
		checkboard[new_y + CHECKBOARD_SIDE * new_y] = getpid();
		sem_signal(data.checkboard_sem, 0);

		log_info(LOG_WRITER_DEVICE, "Device n. %d si è spostato da (%d, %d) a (%d, %d)", 
			number, cur_x, cur_y, new_x, new_y);

		cur_x = new_x;
		cur_y = new_y;

		// LIBERA SEMAFORO TURNO ALTRO
		if (number != DEV_COUNT - 1)
			sem_signal(data.move_sem, number + 1);
	}

	return 0;
}