#pragma once

/**
 *  Struttura dati per condividere handles dal server
 *  al sottoprocesso ack_manager, evita l'uso di variabili
 *  globali
 */
typedef struct ack_manager_data_t {

} ack_manager_data_t;


// Entry point
void ack_manager(ack_manager_data_t data);
