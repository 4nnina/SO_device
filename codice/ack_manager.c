#include "ack_manager.h"

#include "log.h"
#include "defines.h"
#include "shared_memory.h"
#include "semaphore.h"
#include "msg_queue.h"
#include "err_exit.h"

#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/signal.h>
#include <errno.h>
#include <sys/msg.h>
#include <sys/stat.h>

static int msg_queue_id;

static ack_t* ack_list;
static int ack_list_sem;

// Rimuove coda
void ack_manager_callback_sigterm(int sigterm)
{
    log_info("Eliminazione coda messaggi")
    msg_queue_delete(msg_queue_id);

    log_warn("Terminazione ack manager");
    exit(0);
}

// Trova il PID del client che ha usato quel message_id
// Controlla il timestamp 
pid_t find_client(ack_t* list, int message_id)
{
    pid_t result = 0;
    time_t winning = 0xFFFFFFFF;
    for(int i = 0; i < ACK_LIST_MAX_COUNT; ++i)
    {
        ack_t* ack = list + i;
        if (ack->message_id == message_id)
        {
            if (ack->timestamp < winning)
            {
                winning = ack->timestamp;
                result = ack->pid_sender;
            }
        }
    }
    return result;
}

// Crea il messaggio
ack_msg_t create_message(ack_t* list, int message_id)
{
    ack_msg_t result;
    result.type = find_client(list, message_id);
    log_info("CLIENT = %ld", result.type);

    // Accumula messaggi
    int counter = 0;
    for(int i = 0; i < ACK_LIST_MAX_COUNT && counter != DEV_COUNT; ++i)
    {
        ack_t* ack = list + i;
        if (ack->message_id == message_id)
        {
            result.acks[counter] = *ack;
            counter += 1;
        }
    }

    return result;
}

void print_msg_queue(int msg_queue_id)
{
    struct msqid_ds ds;
    if (msgctl(msg_queue_id, IPC_STAT, &ds) == -1)
        panic("Errore ottenimento settings coda");

    log_info("MSGQ: msg_count %ld", ds.msg_qnum);
}

// Invia messaggio nella coda
void send_message(int msg_queue_id, ack_msg_t* message)
{
    size_t size = sizeof(ack_msg_t) - sizeof(long);
    if (msgsnd(msg_queue_id, message, size, 0) == -1)
        panic("Errore invio messaggio");
}

// Ogni 5 secondi
void ack_manager_callback_alarm(int sigalrm)
{
    mutex_lock(ack_list_sem);
    log_info("Controllo lista ack");
    {
        for(int ack_idx = 0; ack_idx < ACK_LIST_MAX_COUNT; ++ack_idx)
        {
            int message_id = ack_list[ack_idx].message_id;
            if (message_id != 0)
            {
                // Se abbiamo finito la propagazione
                if(ack_list_completed(ack_list, message_id))
                {
                    log_info("Accumulo messaggio con id %d da inviare al client", message_id);
                    ack_msg_t result = create_message(ack_list, message_id);

                    for (int i = 0; i < DEV_COUNT; ++i)
                    {
                        ack_t* ack = result.acks + i;
                        log_info("\tMSG %d: sender: %d, receiver: %d",
                            ack->message_id, ack->pid_sender, ack->pid_receiver);
                    }
                    
                    log_info("Invio del messaggio al client %ld", result.type);
                    send_message(msg_queue_id, &result);

                    log_info("Rimozione messaggi con id %d dalla ack lista", message_id);
                    ack_list_remove_all(ack_list, message_id);
                }
            }
        }
    }
    mutex_unlock(ack_list_sem);
    
    alarm(5);
    sleep(10);
} 

// Entry point
void ack_manager(key_t msg_queue_key, ack_manager_data_t data) 
{
    log_set_levels_mask(data.log_level_bits);
	log_set_proc_writer(LOG_WRITER_ACKMAN);

    if (signal(SIGTERM, ack_manager_callback_sigterm) == SIG_ERR || signal(SIGALRM, ack_manager_callback_alarm) == SIG_ERR)
		panic("Errore creazione signal handler");

    ack_list_sem = data.ack_list_sem;
    log_info("Collegamento memoria condivisa");
    ack_list =  shared_memory_attach(data.ack_list_shmem_id, 0, ack_t);

    log_info("Creazione coda messaggi");   
    msg_queue_id = msg_queue_create(msg_queue_key);

    log_info("Inizio controllo");
    
    alarm(5);
    sleep(10);
}