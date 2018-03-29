
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