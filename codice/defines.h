/// @file defines.h
/// @brief Contiene la definizioni di variabili
///         e funzioni specifiche del progetto.

#pragma once

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>

#define MESSAGE_LEN 256

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

	int message_id;
	time_t timestamp;

} ack_t;
