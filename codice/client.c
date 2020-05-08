/// @file client.c
/// @brief Contiene l'implementazione del client.

#include "defines.h"

#include <stdlib.h>

int main(int argc, char * argv[]) {

	message_t message = { 0 };
	message.pid_sender = 9;

	int dev_pid = atoi(argv[1]);

	char filename[128];
	sprintf(filename, "/tmp/dev_fifo.%d", dev_pid);
	int dev_fifo_fd = open(filename, O_WRONLY);

	write(dev_fifo_fd, &message, sizeof(message));

	close(dev_fifo_fd);	

    return 0;
}
