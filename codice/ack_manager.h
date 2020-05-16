#pragma once
#include "defines.h"

/**
 *  Struttura dati per condividere handles dal server
 *  al sottoprocesso ack_manager, evita l'uso di variabili
 *  globali
 */
typedef struct ack_manager_data_t {

    int log_level_bits;

    int ack_list_shmem_id;
    int ack_list_sem;

} ack_manager_data_t;

typedef struct ack_msg_t {
    
    long type;
    ack_t acks[DEV_COUNT];

} ack_msg_t;

// Entry point
void ack_manager(int msg_queue_key, ack_manager_data_t data);
