#pragma once

#define DEV_COUNT 5
#define DEV_MSG_COUNT 32

typedef struct device_data_t {

	int position_file_fd;

	int checkboard_shme_id;
	int ack_list_shmem_id;

	int move_sem;
	int checkboard_sem;
	int ack_list_sem;

} device_data_t;

// Entry point
int device(int number, device_data_t data);