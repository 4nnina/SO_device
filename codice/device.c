#include "device.h"
#include "log.h"
#include "err_exit.h"
#include "semaphore.h"
#include "fifo.h"
#include "shared_memory.h"
#include "defines.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/signal.h>
#include <errno.h>

// Dati globali dei devices
static char device_filename[128];

static int fifo_read_fd;
static int fifo_write_fd;

message_t* get_new_message(message_t* messages) {
	for(int i = 0; i < DEV_MSG_COUNT; ++i)
		if (messages[i].message_id == 0)
			return messages + i;
	return NULL;
}

void remove_message(message_t* message) {
	message->message_id = 0;
}

int can_get_message(message_t* messages) {
	for(int i = 0; i < DEV_MSG_COUNT; ++i)
		if (messages[i].message_id == 0)
			return 1;
	return 0;
}

/**
 *  Rimuove risorse aperte dal device
 */
void device_callback_sigterm(int sigterm) {

	log_info("Chiusura fifo in lettura");
	if (close(fifo_read_fd) == -1)
		panic("Errore chiusura fifo");

	log_info("Chiusura fifo (extra) in scrittura");
	if (fifo_write_fd != 0 && close(fifo_write_fd) == -1)
		panic("Errore chiusura fifo");

	log_info("Eliminazione fifo");
	if(unlink(device_filename))
		panic("Errore unlik fifo");
	
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
	log_set_levels_mask(data.log_level_bits);
	log_set_proc_writer(LOG_WRITER_DEVICE);

	log_info("Figlio n. %d", number);
	
	if (signal(SIGTERM, device_callback_sigterm) == SIG_ERR)
		panic("Errore creazione signal handler");

	sprintf(device_filename, "/tmp/dev_fifo.%d", getpid());
	log_info("Creazione fifo %s", device_filename);
	create_fifo(device_filename);
	
	log_info("Apertura fifo %s in lettura", device_filename);
	fifo_read_fd = open(device_filename, O_RDONLY | O_NONBLOCK);
	if (fifo_read_fd == -1)
		panic("Errore apertura fifo '%s' in lettura", device_filename);
	
	log_info("Apertura fifo (extra) %s in scrittura per mantenerla valida", device_filename);
	fifo_write_fd = open(device_filename, O_WRONLY | O_NONBLOCK);
	if (fifo_write_fd == -1)
		panic("Errore apertura fifo '%s' in scrittura", device_filename);

	// Rendi non bloccante
	int flags = fcntl(fifo_read_fd, F_GETFL, 0);
	if(flags == -1)
		panic("Errore falg read non bloccante")
	if(fcntl(fifo_read_fd, F_SETFL, flags | O_NONBLOCK) == -1)
		panic("Errore settaggio falg non bloccanti");

	// Attach checkboard e ack list
	log_info("Collegamento memoria condivisa del server");
	pid_t* checkboard = shared_memory_attach(data.checkboard_shme_id, 0, pid_t);
	ack_t* ack_list = shared_memory_attach(data.ack_list_shmem_id, 0, ack_t);

	// Messaggi ancora da inviare
	message_t messages_queue[DEV_MSG_COUNT];
	for (int i = 0; i < DEV_MSG_COUNT; ++i)
		messages_queue[i].message_id = 0;

	int cur_x = -1, cur_y = -1;
	while(1) 
	{
		// Nota: Questo pezzo di codice funge sia da inizializzazione che da movimento nei prossimi
		// passi del ciclo

		// ===============================================================================
		// INIZIALIZZAZIONE POSIZIONE E MOVIMENTO

		sem_acquire(data.move_sem, number);

		// Avanza la posizione
		int new_x, new_y;
		get_next_position(data.position_file_fd, &new_x, &new_y);
		
		mutex_lock(data.checkboard_sem);
		{
			static int initialized = 0;
			if (!initialized)
			{
				log_info("Posizione initiale: (%d, %d)", new_x, new_y);
				checkboard[new_x + CHECKBOARD_SIDE * new_y] = getpid();	
				initialized = 1;

				//log_info("Device n. %d è in posizione (%d, %d) all'inizio",
				//	number, new_x, new_y);
			}
			else
			{
				log_info("Mi muovo da (%d, %d) a (%d, %d)", cur_x, cur_y, new_x, new_y);
				checkboard[cur_x + CHECKBOARD_SIDE * cur_y] = 0;
				checkboard[new_x + CHECKBOARD_SIDE * new_y] = getpid();

				//log_info("Device n. %d si è spostato da (%d, %d) a (%d, %d)", 
				//	number, cur_x, cur_y, new_x, new_y);	
			}
		}
		mutex_unlock(data.checkboard_sem);

		cur_x = new_x;
		cur_y = new_y;

		if (number != DEV_COUNT - 1)
			sem_release(data.move_sem, number + 1);


		// INIZIO CICLO DEVICE

		// ===============================================================================
		// INVIO MESSAGGI

		// Prova ad inviare ogni messaggio
		for(int i = 0; i < DEV_MSG_COUNT; ++i) 
		{
			message_t* msg = messages_queue + i;
			if (msg->message_id != 0) // Se valido
			{
				int radius = (int)msg->max_distance; // NOTA: Consideriamo regioni quadrate
				int found = 0;

				mutex_lock(data.checkboard_sem);
				for (int x = MAX(0, cur_x - radius); x < MIN(9, cur_x + radius) && !found; ++x) {
					for (int y = MAX(0, cur_y - radius); y < MIN(9, cur_y + radius) && !found; ++y) 
					{
						pid_t pid = checkboard[x + y * CHECKBOARD_SIDE]; 
						if (pid != 0 && pid != getpid()) 
						{
							// Controlliamo se questo device ha già ricevuto il messaggio
							mutex_lock(data.ack_list_sem);
							int send = !ack_list_exists(ack_list, msg->message_id, pid);
							mutex_unlock(data.ack_list_sem);

							if (send)
							{
								log_info("Invio del messaggio (id %d) da %d a %d", msg->message_id, getpid(), pid);

								message_t output = *msg;
								output.pid_sender = getpid();
								output.pid_receiver = pid;

								// Inviamo messaggio la device
								char filename[128];
								sprintf(filename, "/tmp/dev_fifo.%d", pid);
								
								int output_fifo = open(filename, O_WRONLY);
								if (output_fifo == -1)
									panic("Errore apertura fifo device");

								if (write(output_fifo, &output, sizeof(output)) == -1)
									panic("Errore scrittura fifo device");

								if (close(output_fifo) == -1)
									panic("Errore chiusura fifo device");

								// Rimuove dalla coda dei messaggi
								remove_message(msg);
								found = 1;
							}
						}
					}
				}
				mutex_unlock(data.checkboard_sem);
			}
		}

		// ===============================================================================
		// RICEZIONE MESSAGGIO

		int valid = 1;
		message_t tmp_message;
		while (valid && can_get_message(messages_queue) /* && ack_list ha spazio! */)
		{
			size_t bytes_read = read(fifo_read_fd, &tmp_message, sizeof(tmp_message));
			switch (bytes_read)
			{
				// Messaggio valido
				case sizeof(tmp_message): 
				{
					log_info("Ricevuto un messaggio (id %d)", tmp_message.message_id);
					mutex_lock(data.ack_list_sem);
					
					// Inserisci in lista degli ack
					ack_t* slot = ack_list_get(ack_list);
					message_to_ack(&tmp_message, slot);

					// Controlla se non abbiamo finito
					if (!ack_list_completed(ack_list, tmp_message.message_id))
					{
						log_info("Dobbiamo ancora propagare il messaggio a qualcuno");
						message_t* msg = get_new_message(messages_queue);
						if (msg)
							*msg = tmp_message;
						else
							log_erro("Impossibile ottenere messaggio in device n. %d, index", number);
					}
					else
						log_info("Abbiamo finito di propagare il messaggio");
					
					mutex_unlock(data.ack_list_sem);

				} break;

				case 0:
					log_info("Non ci sono più messaggi");
					valid = 0;
					break;

				// Non ci sono più messaggi
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