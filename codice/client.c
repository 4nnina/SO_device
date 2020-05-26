/// @file client.c
/// @brief Contiene l'implementazione del client.

#include "defines.h"
#include "err_exit.h"
#include "log.h"
#include "msg_queue.h"
#include "ack_manager.h"

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

ack_msg_t wait_message(int msg_queue_id, pid_t pid)
{
	ack_msg_t result;
	size_t size = sizeof(result) - sizeof(result.type);
	if (msgrcv(msg_queue_id, &result, size, pid, 0) == -1)
		panic("Errore attesa messaggio");

	return result;
}

int main(int argc, char * argv[]) 
{
	if (argc < 2)
		panic("Usage: %s msg_queue_key [-iwe]", argv[0]);

	// Setta impostazioni del logger se presenti
	int log_level_bits = 0x0;
	if (argc == 3)
			log_level_bits = log_derive_flags(argv[2]);

	log_set_levels_mask(log_level_bits);
	log_set_proc_writer(LOG_WRITER_CLIENT);

	message_t message = { 0 };
	message.pid_sender = getpid();

	do {
		printf("Inserire pid device ricevente: ");
		scanf("%d%*c", &message.pid_receiver);
	} while(message.pid_receiver <= 0);

	// Controlla storico messages ids
	log_info("Apertura file storico");
	int history_file_fd = open(HISTORY_FILENAME, O_RDWR | O_APPEND, 0);
	if (history_file_fd == -1)
		panic("Errore apertura file storico, probabilmente il server non è attivo");

	int valid = 1;
	do {
		valid = 1;

		printf("Inserire message id: ");
		scanf("%d%*c", &message.message_id);

		// Controlla se il message id è unico
		size_t read_bytes = 1;
		do {
			int used_msg_id = 0;
			read_bytes = read(history_file_fd, &used_msg_id, sizeof(used_msg_id));
			if (read_bytes == -1)
				panic("Errore lettura storico");
			
			// Messaggio già presente
			if (read_bytes != 0 && used_msg_id == message.message_id) {
				log_info("Messaggio ids già usato: %d", used_msg_id);
				valid = 0;
			}
		} while (read_bytes != 0 && valid);

	} while (message.message_id <= 0 || !valid);

	log_info("Scrittura nuovo message id nello storico");
	if (write(history_file_fd, &message.message_id, sizeof(int)) == -1)
		panic("Errore scrittura file storico");

	log_info("Chiusura file storico");
	if (close(history_file_fd) == -1)
		panic("Errore chiusura file storico");

	printf("Inserisci messaggio: ");
	gets(message.message);

	do {
		printf("Inserisci massima distanza di comunicazione: ");
		scanf("%d%*c", &message.max_distance);
	} while(message.max_distance < 1);

	char filename[128];
	sprintf(filename, "/tmp/dev_fifo.%d", message.pid_receiver);

	log_info("Apertuta fifo device");
	int dev_fifo_fd = open(filename, O_CREAT | O_WRONLY);
	if (dev_fifo_fd == -1)
		panic("Errore apertura fifo device");

	log_info("Scrittura messaggio su %s", filename);
	size_t bytes = write(dev_fifo_fd, &message, sizeof(message));
	if (bytes == -1)
		panic("Errore scrittura messaggio");

	log_info("Chiusura fifo");
	if(close(dev_fifo_fd) == -1)
		panic("Errore chiusura fifo");

	// Aspetta messaggio
	key_t msg_queue_key = atoi(argv[1]);

	log_info("Creazione msg queue");
	int msg_queue_id = msg_queue_create(msg_queue_key);

	log_info("Attesa lista acks");
	ack_msg_t message_list = wait_message(msg_queue_id, getpid());
	log_info("Ricevuta lista ack!");
	
	// Scrive su file

	sprintf(filename, "out_message_%d.txt", message.message_id);
	int output_file_fd = open(filename, O_CREAT | O_WRONLY, S_IRUSR | S_IRGRP);
	if (output_file_fd == -1)
		panic("Errore apertura file");

	log_info("Scrittura su %s dell'output", filename);

	char buffer[516];
	sprintf(buffer, "MESSAGGIO %d: %s\n", message.message_id, message.message);
	if(write(output_file_fd, buffer, strlen(buffer)) == -1 ) 
		panic("Errore scrittura in file di testo messaggio");
	
	sprintf(buffer, "Lista acknowledgmend:\n");
	if(write(output_file_fd, buffer, strlen(buffer)) == -1 ) 
		panic("Errore scrittura in file di testo lista acknowledgmend");

	for(int i = 0; i < DEV_COUNT; ++i)
	{
		ack_t* ack = message_list.acks + i;
		struct tm* time = gmtime(&ack->timestamp);

		printf("MSG %d: sender %d, receiver %d, time: %02d:%02d:%02d\n", 
			ack->message_id, ack->pid_sender, ack->pid_receiver, time->tm_hour, time->tm_min, time->tm_sec);

		sprintf(buffer, "\t%6d, %6d, %02d/%02d/%02d %02d:%02d:%02d\n", 
			ack->pid_sender, ack->pid_receiver, time->tm_mday, time->tm_mon, 1900 + time->tm_year, time->tm_hour, time->tm_min, time->tm_sec);

		if(write(output_file_fd, buffer, strlen(buffer)) == -1 ) 
			panic("Errore scrittura in file di testo ack");
	}
	if(close(output_file_fd) == -1)
		panic("Errore chiusura file");
	log_info("Completato");
    return 0;
}
