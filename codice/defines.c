/// @file defines.c
/// @brief Contiene l'implementazione delle funzioni
///         specifiche del progetto.

#include "defines.h"
#include <time.h>

void message_to_ack(message_t* msg, ack_t* ack) 
{
    ack->message_id = msg->message_id;
    ack->pid_receiver = msg->pid_receiver;
    ack->pid_sender = msg->pid_sender;
    ack->timestamp = time(NULL);
}

ack_t* ack_list_get(ack_t* list) {
    for(int i = 0; i < ACK_LIST_MAX_COUNT; ++i)
        if (list[i].message_id == 0) {
            return list + i;
        }

    return NULL;
}

// Rimuove slot
void ack_list_remove(ack_t* slot) {
    slot->message_id = 0;
}

// Ricerca nella lista in base al message_id
int ack_list_exists(ack_t* list, int message_id, pid_t receiver) {
    for (int i = 0; i < ACK_LIST_MAX_COUNT; ++i)
        if (list[i].message_id == message_id && list[i].pid_receiver == receiver)
            return 1;
    return 0;
}

// Controlla se un messaggio è stato inviato a tutti i device
int ack_list_completed(ack_t* list, int message_id) {
    int counter = 0;
    for (int i = 0; i < ACK_LIST_MAX_COUNT && counter != 5; ++i)
        if (list[i].message_id == message_id)
            counter += 1;

    return (counter == 5);
}

void ack_list_remove_all(ack_t* list, int message_id) {
    for(int i = 0; i < ACK_LIST_MAX_COUNT; ++i)
        if (list[i].message_id == message_id)
            ack_list_remove(list + i);
}