/// @file client.c
/// @brief Contiene l'implementazione del client.

#include "defines.h"

#include <stdlib.h>

int main(int argc, char * argv[]) {

	message_t message = { 0 };
	message.pid_sender = getpid();
	message.pid_receiver = atoi(argv[1]);
	message.message_id = 1;
	message.max_distance = 4;

	char filename[128];
	sprintf(filename, "/tmp/dev_fifo.%d", message.pid_receiver);
	int dev_fifo_fd = open(filename, O_WRONLY);

	write(dev_fifo_fd, &message, sizeof(message));
	close(dev_fifo_fd);
    return 0;
}
