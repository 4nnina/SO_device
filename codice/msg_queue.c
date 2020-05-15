#include "msg_queue.h"

#include "err_exit.h"
#include <sys/stat.h>

int msg_queue_create(key_t key)
{
    int flag = IPC_CREAT | S_IWUSR | S_IRUSR;
    int msg_queue = msgget(key, flag);
    if (msg_queue == -1)
        panic("Errore creazione coda messaggi");

    return msg_queue;
}

void msg_queue_delete(int msg_queue_id)
{
    if (msgctl(msg_queue_id, IPC_RMID, NULL) == -1)
        panic("Errore rimozione coda messaggi");
}