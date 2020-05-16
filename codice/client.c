/// @file client.c
/// @brief Contiene l'implementazione del client.

#include "defines.h"
#include "err_exit.h"
#include "msg_queue.h"
#include "ack_manager.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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
	log_set_levels_mask(LOG_LEVEL_INFO_BIT | LOG_LEVEL_WARN_BIT | LOG_LEVEL_ERROR_BIT);
	log_set_proc_writer(LOG_WRITER_SERVER);

	if (argc != 2)
		panic("Usage: %s msg_queue_key", argv[0]);

	message_t message = { 0 };
	message.pid_sender = getpid();

	do {
		printf("Inserire pid device ricevente: ");
		scanf("%d%*c", &message.pid_receiver);
	} while(message.pid_receiver <= 0);

	do {
		printf("Inserire message id: ");
		scanf("%d%*c", &message.message_id);
	} while (message.message_id <= 0);

	printf("Inserisci messaggio: ");
	gets(message.message);

	do {
		printf("Inserisci massima distanza di comunicazione: ");
		scanf("%d%*c", &message.max_distance);
	} while(message.max_distance < 1);

	char filename[128];
	sprintf(filename, "/tmp/dev_fifo.%d", message.pid_receiver);

	printf("Scrittura messaggio su %s\n", filename);
	int dev_fifo_fd = open(filename, O_CREAT | O_WRONLY);
	if (dev_fifo_fd == -1)
		panic("Errore apertura fifo device");

	size_t bytes = write(dev_fifo_fd, &message, sizeof(message));
	if (bytes == -1)
		panic("Errore scrittura messaggio");

	close(dev_fifo_fd);

	// Aspetta messaggio
	key_t msg_queue_key = atoi(argv[1]);

	printf("Creazione msg queue");
	int msg_queue_id = msg_queue_create(msg_queue_key);

	printf("Attesa lista acks\n");
	ack_msg_t message_list = wait_message(msg_queue_id, getpid());
	printf("Ricevuta lista ack!\n");
	
	// Scrive su file

	sprintf(filename, "out_message_%d.txt", message.message_id);
	int output_file_fd = open(filename, O_CREAT | O_WRONLY, S_IRUSR | S_IRGRP);
	if (output_file_fd == -1)
		panic("Errore apertura file");

	printf("Scrittura su %s dell'output\n", filename);

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
		printf("MSG %d: %d, %d\n", ack->message_id, ack->pid_sender, ack->pid_receiver);
		sprintf(buffer, "\t%6d, %6d, %ul\n", ack->pid_sender, ack->pid_receiver, ack->timestamp);
		if(write(output_file_fd, buffer, strlen(buffer)) == -1 ) 
		panic("Errore scrittura in file di testo ack");
	}
	
	close(output_file_fd);
	printf("Fine scrittura\n");

    return 0;
}
