/// @file shared_memory.h
/// @brief Contiene la definizioni di variabili e funzioni
///         specifiche per la gestione della MEMORIA CONDIVISA.

#pragma once

#include <unistd.h>
#include <sys/stat.h>
#include <sys/shm.h>

int create_shared_memory(size_t bytes);
