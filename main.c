
#include <stdio.h>
#include <string.h>

int main(void)
{
	char sendchar = 0x55;

	while(1) {
		putchar(sendchar); // should create square wave
	}

    return 0;
}
