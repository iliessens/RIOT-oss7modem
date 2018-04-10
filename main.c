
#include <stdio.h>
#include <string.h>
#include "shell.h"
#include <stdbool.h>

#include "oss7modem.h"

int readUID(int argc,char** argv) {
	(void) argc;
	(void) argv;
	
	// read 8 bytes from the beginning of the first file
	modem_read_result_t result;
	if(modem_read_file(0,0,8, &result)) {;
	
	printf("\nUID: ");
	for(unsigned int i =0; i< result.length; i++) {
		printf("%02X",result.data[i]);
	}
	printf("\n");
	
	}
	else {
		puts("something went wrong with the reply");
	}
	
	return 0;
}

const shell_command_t shell_commands[] = {
	{"readUID","Read UID of modem",readUID},
	{NULL,NULL,NULL}};

int main(void)
{
	// UARTB
	modem_init(UART_DEV(1));
	
//	uint8_t data[8] = {1,2,3,4,5,6,7,8};
//	modem_write_file(0,0,8,data);
	
	char line_buf[SHELL_DEFAULT_BUFSIZE];
	// won't go past this normally
    shell_run(shell_commands, line_buf, SHELL_DEFAULT_BUFSIZE);
	
    return 0;
}
