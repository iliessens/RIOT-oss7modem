
#include <stdio.h>
#include <string.h>
#include "shell.h"

#include "oss7modem.h"

const shell_command_t shell_commands[] = {
	{NULL,NULL,NULL}};

int main(void)
{
	// UARTB
	modem_init(UART_DEV(1), NULL);
	
	char line_buf[SHELL_DEFAULT_BUFSIZE];
	// won't go past this normally
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
	
    return 0;
}
