#ifndef LOG_H
#define LOG_H

#include "stdint.h"

//Imre's log polyfill
void log_print_string(char* format,...);
void log_print_data(uint8_t* message, uint32_t length);

#endif
