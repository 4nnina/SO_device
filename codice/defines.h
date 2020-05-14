/// @file defines.h
/// @brief Contiene la definizioni di variabili
///         e funzioni specifiche del progetto.

#pragma once

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#define FIFO_READ  0
#define FIFO_WRITE 1

#define MESSAGE_LEN 256
#define DEV_COUNT 5
#define DEV_MSG_COUNT 32

#define CHECKBOARD_SIDE 10
#define ACK_LIST_MAX_COUNT 100

#define MAX(a, b) ((a > b) ? a : b)
#define MIN(a, b) ((a < b) ? a : b)

typedef struct message_t {
	
	pid_t pid_sender;
	pid_t pid_receiver;

	int message_id;
	char message[MESSAGE_LEN];
	
	int max_distance;

} message_t;

typedef struct ack_t {

	pid_t pid_sender;
	pid_t pid_receiver;

	// NOTA: 0 è un valore riservato ed indica che lo slot è libero
	int message_id;
	time_t timestamp;

} ack_t;

// Converte da messaggio ad ack e aggiunge il timestamp
void message_to_ack(message_t* msg, ack_t* ack);

// Ottiene slot libero
// NULL se non ci sono slot liberi
ack_t* ack_list_get(ack_t* list);

// Rimuove slot
void ack_list_remove(ack_t* slot);

// Ricerca nella lista in base al message_id
int ack_list_exists(ack_t* list, int message_id, pid_t receiver); 

// Controlla se un messaggio è stato inviato a tutti i device
int ack_list_completed(ack_t* list, int message_id);

// Rimuove tutti gli slot con lo stesso message id
void ack_list_remove_all(ack_t* list, int message_id);