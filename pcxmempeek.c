#include "ipc.h"

int main(int argc, char **argv)
{
	if(argc != 2) {
		fprintf(stderr, "usage: %s <address in hex>\n", argv[0]);
		return 1;
	}
	PS2Ptr src = strtol(argv[1], NULL, 16);
	uint8_t value;
	ipc_begin();
	ipc_read8(&value, src);
	ipc_send();
	printf("%02x\n", value);
}
