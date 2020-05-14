/// @file err_exit.h
/// @brief Contiene la definizione della funzione di stampa degli errori.

#pragma once

#include <stdio.h>
#include "log.h"

/// @brief Prints the error message of the last failed
///         system call and terminates the calling process.
void ErrExit(const char *msg);

#define panic(...) { log_erro(__VA_ARGS__); ErrExit(NULL); }