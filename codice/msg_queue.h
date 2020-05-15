#pragma once

#include <unistd.h>
#include <sys/msg.h>

int msg_queue_create(key_t key);

void msg_queue_attach(int msg_queue_id);

void msg_queue_delete(int msg_queue_id);