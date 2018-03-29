
#include "log.h"
#include <stdio.h>
#include <stdarg.h>

void log_print_string(char* format,...) {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
	printf("\n");
    va_end(args);
}

void log_print_data(uint8_t* message, uint32_t length)
{
    for( uint32_t i=0 ; i<length ; i++ )
    {
        printf(" %02X", message[i]);
    }
	printf("\n");
}